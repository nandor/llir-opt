(* This file is part of the llir-opt project. *)
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
  | x :: xs ->
    Printf.eprintf "sum: %d\n" x;
    x + sum_vec xs

let test_alloc_many () =
  let rec loop_i = function
    | 0 ->  []
    | i ->
      Printf.eprintf "iter: %d\n" i;
      flush_all ();
      last_vec (build_vec i) :: loop_i (i - 1)
  in
  Printf.eprintf "allocating vector\n";
  let vec = loop_i 500 in
  Printf.eprintf "summing vector\n";
  let s = sum_vec vec in
  Printf.eprintf "done: %d\n" s

let () =
  test_alloc_many ()
