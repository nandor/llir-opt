# This file is part of the genm project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. ALl rights reserved.

OC=ocamlopt.byte
LD=gcc
OPT=../../Release/genm
OCFLAGS=-O2

all: simd

simd.genm: simd.mli simd.ml test.ml simd_ext.c
	$(OC) $(OCFLAGS) $^ -o $@

simd.o: simd.genm
	$(OPT) simd.genm -o=$@ -O2

simd: simd.o
	$(LD) $^ -o $@

.PHONY: clean
clean:
	rm -f *.o *.cmx *.cmi *.genm simd
