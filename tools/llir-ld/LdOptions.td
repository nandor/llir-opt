// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

include "llvm/Option/OptParser.td"



multiclass Eq<string name, string help>
{
  def NAME: Separate<["--", "-"], name>;

  def NAME # _eq:
    Joined<["--", "-"], name # "=">,
    Alias<!cast<Separate>(NAME)>,
    HelpText<help>;
}

multiclass YesNo<string name, string heplYes, string helpNo>
{
  def NAME:
    Flag<["--", "-"], name>,
    HelpText<heplYes>;

  def no_ # NAME:
    Flag<["--", "-"], "no-" # name>,
    HelpText<helpNo>;
}

// Miscellaneous.
def help: Flag<["--", "-"], "help">, HelpText<"Print this help message">;
def v: Flag<["-"], "v">, HelpText<"Display the version number">;

// Flags to specify inputs.
def whole_archive: Flag<["--"], "whole-archive">;
def no_whole_archive: Flag<["--"], "no-whole-archive">;
def start_group: Flag<["--"], "start-group">;
def end_group: Flag<["--"], "end-group">;

defm library_path:
  Eq<"library-path", "Add a directory to the library search path">,
  MetaVarName<"<dir>">;
def:
  JoinedOrSeparate<["-"], "L">,
  Alias<library_path>,
  HelpText<"Alias for --library-path">;

defm library:
  Eq<"library", "Root name of library to use">,
  MetaVarName<"<libName>">;
def:
  JoinedOrSeparate<["-"], "l">,
  Alias<library>,
  HelpText<"Alias for --library">;

// Flags to control output.
def shared:
  Flag<["-", "--"], "shared">,
  HelpText<"Build a shared object">;

def static:
  Flag<["-", "--"], "static">,
  HelpText<"Do not link against shared libraries">;

def relocatable:
  Flag<["-", "--"], "relocatable">,
  HelpText<"Create relocatable object file">;
def:
  Flag<["-"], "r">,
  Alias<relocatable>,
  HelpText<"Alias for --relocatable">;

defm entry:
  Eq<"entry", "Name of entry point symbol">;
def:
  JoinedOrSeparate<["-"], "e">,
  Alias<entry>,
  HelpText<"Alias for --entry">;

def o:
  JoinedOrSeparate<["-"], "o">,
  MetaVarName<"<path>">,
  HelpText<"Output path">;

defm undefined:
  Eq<"undefined", "Force undefined symbol during linking">,
  MetaVarName<"<symbol>">;

def:
  JoinedOrSeparate<["-"], "u">,
  Alias<undefined>,
  HelpText<"Alias for --undefined">;

// Irrelevant options.
def nostdlib: Flag<["-"], "nostdlib">;
def gc_sections: Flag<["--"], "gc-sections">;

// Options which are ignored or simply forwarded.
defm soname:
  Eq<"soname", "Set DT_SONAME">;

def m:
  JoinedOrSeparate<["-"], "m">,
  HelpText<"Choose an emulation">,
  Flags<[RenderJoined]>;

defm hash_style:
  Eq<"hash-style", "Specify hash style (sysv, gnu or both)">;

defm export_dynamic: YesNo
  < "export-dynamic"
  , "Export symbols in the dynamic symbol table"
  , "Do not export symbols in the dynamic symbol table"
  >;
def:
  Flag<["-"], "E">,
  Alias<export_dynamic>,
  HelpText<"Alias for --export-dynamic">;

def discard_locals:
  Flag<["--", "-"], "discard-locals">,
  HelpText<"Delete temporary local symbols">;
def:
  Flag<["-"], "X">,
  Alias<discard_locals>,
  HelpText<"Alias for --discard-locals">;

defm dynamic_linker:
  Eq<"dynamic-linker", "Which dynamic linker to use">;

def z:
  JoinedOrSeparate<["-"], "z">,
  HelpText<"Linker option extensions">;

defm version_script:
  Eq<"version-script", "Specify a version script to use">;

defm script:
  Eq<"script", "Specify linker scripts to use">;
def:
  JoinedOrSeparate<["-"], "T">,
  Alias<script>,
  HelpText<"Alias for --script">;

def default_script: Flag<["--"], "default-script">;
def:
  JoinedOrSeparate<["-"], "dT">,
  Alias<default_script>;

defm rpath:
  Eq<"rpath", "Add DT_RUNPATH">;
def:
  JoinedOrSeparate<["-"], "R">,
  Alias<rpath>,
  HelpText<"Alias for --rpath">;
defm rpath_link:
  Eq<"rpath-link", "">;

def Bstatic: Flag<["-", "--"], "Bstatic">, HelpText<"Do not link against shared libraries">;

def Bdynamic: Flag<["-", "--"], "Bdynamic">, HelpText<"Link against shared libraries">;

def Ttext_segment:
  Separate<["-", "--"], "Ttext-segment">;
def:
  Joined<["-", "--"], "Ttext-segment=">,
  Alias<Ttext_segment>;

def Bsymbolic: Flag<["-", "--"], "Bsymbolic">;

def as_needed: Flag<["--"], "as-needed">;
def no_as_needed: Flag<["--"], "no-as-needed">;
def eh_frame_hdr: Flag<["--"], "eh-frame-hdr">;
def d: Flag<["-"], "d">;
def fatal_warnings: Flag<["--"], "fatal-warnings">;

def build_id: Flag<["--"], "build-id">;
def build_id_eq: Joined<["--"], "build-id=">;

def omagic: Flag<["--"], "omagic">;
def: Flag<["-"], "N">, Alias<omagic>;

defm Map: Eq<"Map", "Print a link map to the specified file">;

defm cref: YesNo
  < "cref"
  , "Output cross reference table"
  , "Do not output cross reference table"
  >;

defm unresolved_symbols: Eq<"unresolved-symbols", "Determine how to handle unresolved symbols">;

/// LLIR-specific.
defm mcpu: Eq<"mcpu", "Specify the target processor">;
defm mabi: Eq<"mabi", "Specify the target ABI">;
defm mfs: Eq<"mfs", "Specify the target feature string">;

def O_Group:
  OptionGroup<"<O group>">,
  HelpText<"Optimization level">;

def O0 : Flag<["-"], "O0">, Group<O_Group>;
def O1 : Flag<["-"], "O1">, Group<O_Group>;
def O2 : Flag<["-"], "O2">, Group<O_Group>;
def O3 : Flag<["-"], "O3">, Group<O_Group>;
def O4 : Flag<["-"], "O4">, Group<O_Group>;
def Os : Flag<["-"], "Os">, Group<O_Group>;
