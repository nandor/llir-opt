(* This file is part of the genm-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external exc_throw : int -> unit = "exc_throw"

(*
let test_throw_from_c () =
  try exc_throw 6; assert false
  with _ -> ()
*)
let () =
  print_endline "Running test"
  (*test_throw_from_c ()*)

