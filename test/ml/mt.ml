(* This file is part of the llir-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)


let thread1 () = print_endline "thread1"

let thread2 () = print_endline "thread2"


let () =
  Thread.join (Thread.create thread1 ());
  Thread.join (Thread.create thread2 ());
