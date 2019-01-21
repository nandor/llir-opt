(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t



val make : float -> float -> t
val add : t -> t -> t
val sub : t -> t -> t
val mul : t -> t -> t
val conj : t -> t
