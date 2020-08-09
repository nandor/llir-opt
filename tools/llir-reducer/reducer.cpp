// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <mutex>
#include <set>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Threading.h>

#include "core/bitcode.h"
#include "core/clone.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/parser.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/util.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_data_elim.h"
#include "passes/dead_func_elim.h"
#include "passes/move_elim.h"
#include "passes/sccp.h"
#include "passes/simplify_cfg.h"
#include "passes/stack_object_elim.h"
#include "passes/undef_elim.h"
#include "passes/verifier.h"
#include "job_runner.h"
#include "inst_reducer.h"
#include "timeout.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;
using WithColor = llvm::WithColor;



// -----------------------------------------------------------------------------
static const char *kTool = "llir-reducer";



// -----------------------------------------------------------------------------
static cl::opt<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<std::string>
optTest("test", cl::desc("test script"), cl::Required);

static cl::opt<unsigned>
optThreads("j", cl::init(llvm::hardware_concurrency()));

static cl::opt<unsigned>
optPool("pool", cl::init(10));

static cl::opt<unsigned>
optStop("stop", cl::init(20));

static cl::opt<bool>
optVerbose("verbose", cl::init(false));

static cl::opt<bool>
optCheckpoint("checkpoint", cl::init(false));

static cl::opt<std::string>
optReducers("reducers", cl::init("symbol,block,inst,symbol"), cl::Hidden);

static cl::opt<unsigned>
optTimeout("timeout", cl::desc("timeout in seconds"), cl::init(0));



// -----------------------------------------------------------------------------
static llvm::Expected<bool> Verify(const Prog &prog)
{
  // Create a temp file and dump the program to it.
  auto tmp = sys::fs::TempFile::create("llir-reducer-%%%%%%%.llbc");
  if (!tmp) {
    return tmp.takeError();
  }

  llvm::raw_fd_ostream os(tmp->FD, false);
  BitcodeWriter(os).Write(prog);
  os.flush();

  // Run the verifier script, providing no stdin and ignoring stdout/stderr.
  llvm::StringRef args[] = {
      optTest.c_str(),
      tmp->TmpName.c_str()
  };
  llvm::Optional<llvm::StringRef> redir[] = { { "" }, { "" }, { "" } };
  std::string msg;
  auto code = sys::ExecuteAndWait(args[0], args, llvm::None, redir, 0, 0, &msg);

  // Discard the file.
  if (auto err = tmp->discard()) {
    return std::move(err);
  }

  // Test succeeded if it returned 0.
  return code == 0;
}


// -----------------------------------------------------------------------------
static bool Write(const Prog &prog)
{
  // Open the output stream.
  std::error_code err;
  auto output = std::make_unique<llvm::ToolOutputFile>(
      optOutput,
      err,
      sys::fs::F_Text
  );
  if (err) {
    WithColor::error(llvm::errs(), kTool) << err.message() << "\n";
    return false;
  }

  // Emit the simplified file.
  BitcodeWriter(output->os()).Write(prog);
  output->keep();
  return true;
}


// -----------------------------------------------------------------------------
static size_t Size(const Func &func)
{
  size_t size = 0;
  for (const Block &block : func) {
    size += block.size();
  }
  return size;
}

// -----------------------------------------------------------------------------
static size_t Size(const Prog &prog)
{
  size_t size = 0;
  for (const Func &func : prog) {
    size += 1;
    size += Size(func);
  }
  for (const Data &data : prog.data()) {
    size += 1;
    for (const Object &object : data) {
      size += 1;
      for (const Atom &atom : object) {
        size += 1;
        for (const Item &item : atom) {
          size += 1;
        }
      }
    }
  }
  return size;
}

// -----------------------------------------------------------------------------
class GlobalReducer {
private:
  /// Symbols to delete from the program.
  using Task = std::set<std::string_view>;

  struct Result {
    /// Reduced program.
    std::unique_ptr<Prog> Program;
    /// Erased symbols.
    std::set<std::string_view> Deleted;
    /// Size of the reduced program.
    size_t Size;
    /// Flag indicated whether the program passes the test.
    bool Pass;
    /// Sequence number.
    uint64_t ID;

    /// Ordering, increasing by size.
    bool operator < (const Result &that) const {
      if (Size == that.Size) {
        return Deleted.size() > that.Deleted.size();
      } else {
        return Size < that.Size;
      }
    }
  };

  class JobRunnerImpl final : public JobRunner<Task, Result> {
  public:
    /// Initialiess the job runner.
    JobRunnerImpl(
        GlobalReducer *reducer,
        std::unique_ptr<Prog> &&prog,
        const char *name)
      : JobRunner<Task, Result>(optThreads)
      , reducer_(reducer)
      , origin_(std::move(prog))
      , uid_(0ull)
      , cnt_(0ull)
      , name_(name)
    {
    }

    /// Returns the best candidate.
    std::unique_ptr<Prog> GetBest()
    {
      if (optVerbose) {
        llvm::outs() << "\n";
      }
      if (reduced_.empty()) {
        return std::move(origin_);
      } else {
        return std::move(reduced_[0].Program);
      }
    }

  private:
    /// Request a task to run.
    std::optional<Task> Request() override
    {
      // Initialise the set of symbols.
      if (symbols_.empty()) {
        symbols_ = reducer_->Enumerate(*origin_);
        if (symbols_.empty()) {
          return std::nullopt;
        }
      }

      // Check if reduction should stop.
      if (reduced_.empty()) {
        if (uid_ > optStop) {
          return std::nullopt;
        }
      } else {
        if (uid_ - reduced_[0].ID > optStop) {
          // Stop if no change happened.
          return std::nullopt;
        }
      }

      // Keep going - conjure up a set of functions to delete.
      std::set<std::string_view> Deleted;
      if (reduced_.size() < optPool || std::bernoulli_distribution(0.5)(rand_)) {
        // Seed by randomly selecting a set.
        if (!reduced_.empty()) {
          auto r = std::uniform_int_distribution<>(0, reduced_.size() - 1)(rand_);
          std::copy(
              reduced_[r].Deleted.begin(),
              reduced_[r].Deleted.end(),
              std::inserter(Deleted, Deleted.end())
          );
        }

        // Add at most 10% of the functions.
        std::vector<std::string_view> diff;
        std::set_difference(
            symbols_.begin(), symbols_.end(),
            Deleted.begin(), Deleted.end(),
            std::back_inserter(diff)
        );
        if (diff.empty()) {
          return std::nullopt;
        }

        std::shuffle(diff.begin(), diff.end(), rand_);
        auto n = std::max(
            1,
            std::uniform_int_distribution<>(
              diff.size() * 0.05,
              diff.size() * 0.30
          )(rand_)
        );
        std::copy(
            diff.begin(),
            diff.begin() + n,
            std::inserter(Deleted, Deleted.end())
        );
      } else {
        // Try to combine two sets.
        auto r0 = std::uniform_int_distribution<>(
            0,
            reduced_.size() / 2 - 1
        )(rand_);
        auto r1 = reduced_.size() - r0 - 1;

        // Copy the first deleted set.
        std::copy(
            reduced_[r0].Deleted.begin(),
            reduced_[r0].Deleted.end(),
            std::inserter(Deleted, Deleted.end())
        );

        // Copy a random number of elements from the second.
        // Try to select enough elements to get a set larger
        // than the best result so far.
        std::vector<std::string_view> del(
          reduced_[r1].Deleted.begin(),
          reduced_[r1].Deleted.end()
        );
        std::shuffle(del.begin(), del.end(), rand_);

        int bestSize = reduced_[0].Deleted.size();
        int minimum = std::min<int>(
            del.size(),
            std::max<int>(0, bestSize - reduced_[r0].Deleted.size())
        );
        auto n = std::uniform_int_distribution<>(
            std::max<int>(minimum, del.size() / 2),
            del.size()
        )(rand_);
        std::copy(
            del.begin(),
            del.begin() + n,
            std::inserter(Deleted, Deleted.end())
        );
      }
      return Deleted;
    }

    /// Prepare the program and run the reducer on a separate thread.
    Result Run(Task &&task) override;

    /// Record the result and write the output if new result is better.
    void Post(Result &&result) override
    {
      cnt_++;

      if (reduced_.empty()) {
        Display(cnt_, Size(*origin_));
      } else {
        Display(cnt_, reduced_.begin()->Size);
      }

      if (!result.Pass) {
        return;
      }

      std::optional<unsigned> oldBest;
      if (!reduced_.empty()) {
        oldBest = reduced_.begin()->Size;
      }

      // Result is always sorted, keep it that way.
      reduced_.insert(
        std::upper_bound(
          reduced_.begin(),
          reduced_.end(),
          result,
          [](const Result &a, const Result &b) { return a.Size < b.Size; }
        ),
        std::move(result)
      );

      // Write if first result or this one is better.
      if (!oldBest || *oldBest > reduced_.begin()->Size) {
        if (optCheckpoint) {
          Write(*reduced_.begin()->Program);
        }
      }

      // Keep the pool at a manageable size.
      if (reduced_.size() > optPool) {
        reduced_.erase(reduced_.begin() + optPool, reduced_.end());
      }
    }

    /// Display some progress information.
    void Display(uint64_t cnt, uint64_t best)
    {
      if (!optVerbose) {
        return;
      }
      llvm::outs()
          << "\rReduce " << name_ << ": "
          << "iteration " << llvm::format("%6d", cnt) << ", "
          << "best " << llvm::format("%9d", best);
      llvm::outs().flush();
    }

  private:
    /// Callbacks for the reducer.
    GlobalReducer *reducer_;
    /// Original program.
    std::unique_ptr<Prog> origin_;
    /// List of all function names, sorted by size.
    std::set<std::string_view> symbols_;
    /// Random generator.
    std::mt19937 rand_;
    /// List of reduced programs.
    std::vector<Result> reduced_;
    /// Sequential unique ID.
    std::atomic<uint64_t> uid_;
    /// Number of completed candidates.
    std::atomic<uint64_t> cnt_;
    /// Name of the reducer.
    const char *name_;
  };

public:
  GlobalReducer(std::unique_ptr<Prog> &&prog, const char *name)
    : runner_(this, std::move(prog), name)
  {
  }

  std::unique_ptr<Prog> Run(const Timeout &timeout)
  {
    runner_.Execute(timeout);
    return runner_.GetBest();
  }

  /// Enumerate all reduction candidates.
  virtual std::set<std::string_view> Enumerate(const Prog &prog) = 0;

  /// Delete the symbols.
  virtual void Reduce(Prog &prog, std::set<std::string_view> Deleted) = 0;

private:
  /// Parallel job executor.
  JobRunnerImpl runner_;
};

// -----------------------------------------------------------------------------
GlobalReducer::Result GlobalReducer::JobRunnerImpl::Run(Task &&task)
{
  /// Clone the program.
  std::unique_ptr<Prog> program = Clone(*origin_);

  /// Remove the indicated functions.
  reducer_->Reduce(*program, task);

  /// Simplify the program.
  PassManager mngr(false, false);
  mngr.Add<VerifierPass>();
  mngr.Add<StackObjectElimPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<DeadDataElimPass>();
  mngr.Add<VerifierPass>();
  mngr.Run(*program);

  // Run the verifier.
  Result result;
  if (auto flagOrError = Verify(*program)) {
    result.Pass = *flagOrError;
  } else {
    consumeError(flagOrError.takeError());
    WithColor::error(llvm::errs(), kTool) << "failed to run verifier";
    result.Pass = false;
  }

  result.Deleted = std::move(task);
  result.Program = std::move(program);
  result.Size = Size(*result.Program);
  result.ID = ++uid_;
  return result;
}

// -----------------------------------------------------------------------------
static void ReduceFunc(Prog &prog, std::set<std::string_view> Deleted)
{
  Func *first = nullptr;
  for (auto it = prog.begin(); it != prog.end(); ) {
    Func *f = &*it++;
    if (Deleted.count(f->GetName())) {
      f->SetVisibility(Visibility::HIDDEN);
      if (f->use_empty()) {
        f->eraseFromParent();
      } else {
        if (first) {
          f->replaceAllUsesWith(first);
          f->eraseFromParent();
        } else {
          f->clear();
          auto *bb = new Block((".L" + f->getName() + "_entry").str());
          bb->AddInst(new TrapInst({}));
          f->AddBlock(bb);
          first = f;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
static void ReduceData(Prog &prog, std::set<std::string_view> Deleted)
{
  for (auto dt = prog.data_begin(); dt != prog.data_end(); ) {
    Data *data = &*dt++;

    Object *firstObj = nullptr;
    for (auto ot = data->begin(); ot != data->end(); ) {
      Object *obj = &*ot++;

      Atom *firstAtom = nullptr;
      for (auto it = obj->begin(); it != obj->end(); ) {
        Atom *a = &*it++;
        a->SetVisibility(Visibility::HIDDEN);
        if (a->use_empty()) {
          a->eraseFromParent();
        } else if (Deleted.count(a->GetName())) {
          if (firstAtom) {
            a->replaceAllUsesWith(firstAtom);
            a->eraseFromParent();
          } else {
            a->clear();
            firstAtom = a;
          }
        }
      }
      if (obj->size() == 1) {
        Atom *atom = &*obj->begin();
        if (atom->use_empty()) {
          // No uses - delete object.
          obj->eraseFromParent();
        } else if (atom->empty()) {
          // Coalesce with a previous one.
          if (firstObj) {
            atom->replaceAllUsesWith(&*firstObj->begin());
            atom->eraseFromParent();
          } else {
            firstObj = obj;
          }
        }
      }
    }
    if (data->empty()) {
      data->eraseFromParent();
    }
  }
}

// -----------------------------------------------------------------------------
class FuncReducer : public GlobalReducer {
public:
  FuncReducer(std::unique_ptr<Prog> &&prog)
    : GlobalReducer(std::move(prog), "functions")
  {
  }

  std::set<std::string_view> Enumerate(const Prog &prog) override
  {
    std::set<std::string_view> functions;
    for (const Func &func : prog) {
      functions.insert(func.GetName());
    }
    return functions;
  }

  void Reduce(Prog &prog, std::set<std::string_view> deleted) override
  {
    ReduceFunc(prog, deleted);
  }
};

// -----------------------------------------------------------------------------
class AtomReducer : public GlobalReducer {
public:
  AtomReducer(std::unique_ptr<Prog> &&prog)
    : GlobalReducer(std::move(prog), "atoms")
  {
  }

  std::set<std::string_view> Enumerate(const Prog &prog) override
  {
    std::set<std::string_view> atoms;
    for (const Data &data : prog.data()) {
      for (const Object &object : data) {
        for (const Atom &atom : object) {
          atoms.insert(atom.GetName());
        }
      }
    }
    return atoms;
  }

  void Reduce(Prog &prog, std::set<std::string_view> deleted) override
  {
    ReduceData(prog, deleted);
  }
};

// -----------------------------------------------------------------------------
class SymbolReducer : public GlobalReducer {
public:
  SymbolReducer(std::unique_ptr<Prog> &&prog)
    : GlobalReducer(std::move(prog), "atoms and functions")
  {
  }

  std::set<std::string_view> Enumerate(const Prog &prog) override
  {
    std::set<std::string_view> symbols;
    for (const Data &data : prog.data()) {
      for (const Object &object : data) {
        for (const Atom &atom : object) {
          symbols.insert(atom.GetName());
        }
      }
    }
    for (const Func &func : prog) {
      symbols.insert(func.GetName());
    }
    return symbols;
  }

  void Reduce(Prog &prog, std::set<std::string_view> deleted) override
  {
    ReduceFunc(prog, deleted);
    ReduceData(prog, deleted);
  }
};

// -----------------------------------------------------------------------------
class BlockReducer : public GlobalReducer {
public:
  BlockReducer(std::unique_ptr<Prog> &&prog)
    : GlobalReducer(std::move(prog), "blocks")
  {
  }

  std::set<std::string_view> Enumerate(const Prog &prog) override
  {
    std::set<std::string_view> symbols;
    for (const Func &func : prog) {
      for (const Block &block : func) {
        if (block.size() == 1) {
          continue;
        }
        if (block.begin()->Is(Inst::Kind::TRAP)) {
          continue;
        }
        if (&block == &func.getEntryBlock()) {
          continue;
        }
        symbols.insert(block.GetName());
      }
    }
    return symbols;
  }

  void Reduce(Prog &prog, std::set<std::string_view> deleted) override
  {
    for (Func &func : prog) {
      for (Block &block : func) {
        if (deleted.count(block.GetName())) {
          for (Block *succ : block.successors()) {
            for (PhiInst &phi : succ->phis()) {
              if (phi.HasValue(&block)) {
                phi.Remove(&block);
              }
            }
          }
          block.clear();
          block.AddInst(new TrapInst({}));
        }
      }
      func.RemoveUnreachable();
    }
  }
};

// -----------------------------------------------------------------------------
class InstReducer : public InstReducerBase {
public:
  InstReducer() 
    : InstReducerBase(optThreads)
  {
    if (optVerbose) {
      llvm::outs() << "Reduce instructions: ";
    }
  }

  ~InstReducer()
  {
    if (optVerbose) {
      llvm::outs() << "\n";
    }
  }

  bool Verify(const Prog &prog) const override
  {
    if (auto flagOrError = ::Verify(prog)) {
      if (*flagOrError) {
        if (optVerbose) {
          llvm::outs()
              << "\rReduce instructions: " << llvm::format("%9d", Size(prog));
          llvm::outs().flush();
        }
        if (optCheckpoint) {
          Write(prog);
        }
        return true;
      }
      return false;
    } else {
      consumeError(flagOrError.takeError());
      WithColor::error(llvm::errs(), kTool) << "failed to run verifier";
      return false;
    }
  }
};

// -----------------------------------------------------------------------------
static std::unique_ptr<Prog> Reduce(std::unique_ptr<Prog> &&prog)
{
  llvm::SmallVector<llvm::StringRef, 3> reducers;
  llvm::StringRef(optReducers).split(reducers, ",", -1, false);

  Timeout timeout(optTimeout);
  for (const llvm::StringRef reducer : reducers) {
    if (reducer == "symbol") {
      prog = SymbolReducer(std::move(prog)).Run(timeout);
      continue;
    }
    if (reducer == "block") {
      prog = BlockReducer(std::move(prog)).Run(timeout);
      continue;
    }
    if (reducer == "func") {
      prog = FuncReducer(std::move(prog)).Run(timeout);
      continue;
    }
    if (reducer == "atom") {
      prog = AtomReducer(std::move(prog)).Run(timeout);
      continue;
    }
    if (reducer == "inst") {
      prog = InstReducer().Reduce(std::move(prog), timeout);
      continue;
    }

    llvm::errs() << "Uknown reducer: " << reducer << "\n";
    return nullptr;
  }
  return std::move(prog);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(optInput);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the input program.
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  std::unique_ptr<Prog> prog(Parse(buffer, "llir-reduce"));
  if (!prog) {
    return EXIT_FAILURE;
  }

  // Run the reducer.
  if (auto reduced = Reduce(std::move(prog))) {
    Write(*reduced);
    return EXIT_SUCCESS;
  } else {
    WithColor::error(llvm::errs(), kTool) << "failed to reduce\n";
    return EXIT_FAILURE;
  }
}
