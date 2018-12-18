(* This file is part of the genm-opt project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

external gc_ident : 'a -> 'a = "gc_ident"


type struct_1 = { field1_0: int; }
type struct_2 = { field2_0: int; field2_1: int; }
type struct_3 = { field3_0: int; field3_1: int; field3_2: int; }
type struct_4 = { field4_0: int; field4_1: int; field4_2: int; field4_3: int; }


let test_alloc_size () =
  let s_1 = gc_ident
    { field1_0 = gc_ident 2;
    }
  in
  let s_2 = gc_ident
    { field2_0 = gc_ident 1
    ; field2_1 = gc_ident 3
    }
  in
  let s_3 = gc_ident
    { field3_0 = gc_ident 2
    ; field3_1 = gc_ident 5
    ; field3_2 = gc_ident 6
    }
  in
  let s_4 = gc_ident
    { field4_0 = gc_ident 3
    ; field4_1 = gc_ident 8
    ; field4_2 = gc_ident 5
    ; field4_3 = gc_ident 9
    }
  in
  assert (2  == s_1.field1_0);
  assert (4  == s_2.field2_0 + s_2.field2_1);
  assert (13 == s_3.field3_0 + s_3.field3_1 + s_3.field3_2);
  assert (25 == s_4.field4_0 + s_4.field4_1 + s_4.field4_2 + s_4.field4_3)

let () =
  test_alloc_size ()
