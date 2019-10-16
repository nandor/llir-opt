(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)

type t

val make  : float -> float -> float -> float -> t
val zero  : t
val scale : t -> float -> t
val dot   : t -> t -> float
