(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t
type vec


external make           : float -> float -> t     = "complex_make"
external re             : t -> float              = "complex_re"
external im             : t -> float              = "complex_im"
external add            : t -> t -> t             = "complex_add"
external sub            : t -> t -> t             = "complex_sub"
external mul            : t -> t -> t             = "complex_mul"
external exp            : t -> t                  = "complex_exp"
external abs            : t -> float              = "complex_abs"
external vec_make       : int -> vec              = "complex_vec_make"
external vec_length     : vec -> int              = "complex_vec_length"     [@@noalloc]
external vec_unsafe_get : vec -> int -> t         = "complex_vec_unsafe_get"
external vec_unsafe_set : vec -> int -> t -> unit = "complex_vec_unsafe_set" [@@noalloc]


let zero = make 0.0 0.0

let vec_dup vo =
  let n = vec_length vo in
  let vn = vec_make n in
  for i = 0 to n - 1 do
    vec_unsafe_set vn i (vec_unsafe_get vo i);
  done;
  vn

let vec_init n f =
  let v = vec_make n in
  for i = 0 to n - 1 do
    vec_unsafe_set v i (f i);
  done;
  v

let vec_get v i =
  if i < 0 || vec_length v <= i then
    raise (Invalid_argument "index out of bounds");
  vec_unsafe_get v i

let vec_set v i e =
  if i < 0 || vec_length v <= i then
    raise (Invalid_argument "index out of bounds");
  vec_unsafe_set v i e
