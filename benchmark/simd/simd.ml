(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t

external make  : float -> float -> float -> float -> t = "simd_make"
external scale : t -> float -> t                       = "simd_scale"
external dot   : t -> t -> float                       = "simd_dot"

let zero = make 0.0 0.0 0.0 0.0
