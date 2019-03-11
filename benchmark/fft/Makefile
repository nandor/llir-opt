# This file is part of the genm project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. ALl rights reserved.

OC=ocamlopt.byte
LD=gcc
OPT=../../Release/genm
OCFLAGS=-O2

all: fft

fft.genm: complex.mli complex.ml fft.mli fft.ml test.ml complex_ext.c
	$(OC) $(OCFLAGS) $^ -o $@

fft.o: fft.genm
	$(OPT) fft.genm -o=$@ -O2

fft: fft.o
	$(LD) $^ -o $@

.PHONY: clean
clean:
	rm -f *.o *.cmx *.cmi *.genm fft
