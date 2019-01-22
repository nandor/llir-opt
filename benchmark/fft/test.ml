(* This file is part of the genm project. *)
(* Licensing information can be found in the LICENSE file. *)
(* (C) 2018 Nandor Licker. ALl rights reserved. *)



let () =
  let freqs = [2.; 5.; 11.; 17.; 29.] in
  let n = 64 in
  let x = Complex.vec_init n (fun i ->
    let r = float_of_int i /. float_of_int n in
    Complex.make (List.fold_left (+.) 0.0 (List.map (fun freq ->
      sin (2. *. Float.pi *. freq *. r)
    ) freqs)) 0.0
  )
  in
  let v = Fft.fft x in
  for i = 0 to n do
    let xi = Complex.re (Complex.vec_get x i)in
    let vi = Complex.abs (Complex.vec_get v i) in
    Printf.eprintf "% 3d\t%+ 2.5f\t% 2.5f\n" i xi vi;
  done
