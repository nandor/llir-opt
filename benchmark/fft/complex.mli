(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t
type vec


val make : float -> float -> t
val zero : t
val re : t -> float
val im : t -> float
val add : t -> t -> t
val sub : t -> t -> t
val mul : t -> t -> t
val exp : t -> t
val abs : t -> float


val vec_make : int -> vec
val vec_dup : vec -> vec
val vec_init : int -> (int -> t) -> vec
val vec_length : vec -> int
val vec_get : vec -> int -> t
val vec_set : vec -> int -> t -> unit
