(* This file is part of the genm-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external exc_throw : int -> unit = "exc_throw"

let test_throw_from_c () =
  let throw = ref true in
  let catch = ref false in
  begin
    try
      exc_throw 6;
      throw := false
    with
      _ ->
      catch := true
  end;
  assert (!throw && !catch)

let () =
  test_throw_from_c ()

