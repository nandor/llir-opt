(* This file is part of the genm-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external exc_throw : int -> unit = "exc_throw"
external exc_get_0 : unit -> int = "exc_get_0"
external exc_ident : 'a -> 'a = "exc_ident"

let test_throw_from_c () =
  let throw = ref true in
  let catch = ref false in
  begin
    try
      exc_throw 6;
      throw := false
    with _ -> catch := true
  end;
  assert (!throw && !catch)

let test_throw_from_ml () =
  let helper () =
    print_int (1 / exc_get_0 ())
  in
  let throw = ref true in
  let catch = ref false in
  begin
    try
      helper ();
      throw := false
    with _ -> catch := true
  end;
  assert (!throw && !catch)

let test_throw_bound_error () =
  let helper arr i =
    print_int (arr.(i))
  in
  let throw = ref true in
  let catch = ref false in
  begin
    try
      let arr = [| 1; 2; 3; 4 |] in
      let idx = exc_ident 100 in
      helper arr idx;
      throw := false
    with _ -> catch := true
  end;
  assert (!throw && !catch)

let () =
  test_throw_from_c ();
  test_throw_from_ml ()

