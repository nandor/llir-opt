(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external id : 'a -> 'a = "%identity"

let make_const f = Simd.make f f f f

let test x =
  (*let n = 4 in
  let acc = ref 0. in
  for i = 0 to n - 1 do
    let f = 4. *. float_of_int i in
    let v0 = Simd.make (f +. 0.) (f +. 1.) (f +. 2.) (f +. 3.) in
    let v1 = Simd.scale v0 2. in
    let f = Simd.dot v0 v1 in
    acc := !acc +. f
  done;
  print_float !acc*)

  let v0 = make_const (float_of_int x) in
  let v1 = make_const (float_of_int x) in
  let d = Simd.dot v0 v1 in
  ignore d

let () = test 1
