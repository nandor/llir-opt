(* This file is part of the genm-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external gc_ident : 'a -> 'a = "gc_ident"

let rec build_vec n =
  let rec loop = function
    | 0 -> []
    | m -> n - m :: loop (m - 1)
  in loop n

let rec last_vec = function
  | [] -> failwith "empty vector"
  | [x] -> x
  | x :: xs -> last_vec xs

let rec sum_vec = function
  | [] -> 0
  | x :: xs -> x + sum_vec xs

let test_alloc_many () =
  let rec loop_i = function
    | 0 ->  []
    | i -> last_vec (build_vec i) :: loop_i (i - 1)
  in print_int (sum_vec (loop_i 10))

let () =
  test_alloc_many ()
