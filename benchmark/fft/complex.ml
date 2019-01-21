(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t



external make : float -> float -> t = "complex_make"
external add : t -> t -> t = "complex_add"
external sub : t -> t -> t = "complex_sub"
external mul : t -> t -> t = "complex_mul"
external conj : t -> t = "complex_conj"
