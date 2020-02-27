(* This file is part of the llir-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external double_ident : 'a -> 'a = "double_ident"


let () =
  assert(double_ident 1. +. double_ident 1. = double_ident 2.)
