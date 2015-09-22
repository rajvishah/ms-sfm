# Makefile for CompareModels
# Author: Noah Snavely
# Copyright (c) 2011 Cornell University

LAPACK_BLAS_DIR=-L/usr/lib64/atlas/ -L/global/OpenBLAS/lib/ -L/home/cvit/rajvi/bundler/lib/

CC=gcc
OPTFLAGS=-g2 -Wall

INCLUDE_PATH=-Iimagelib -Imatrix -I/home/cvit/rajvi/bundler/include/

LIB_PATH=-Limagelib -Lmatrix $(LAPACK_BLAS_DIR)

CPPFLAGS=$(OPTFLAGS) $(INCLUDE_PATH)

LIBS=-limage -lmatrix -llapack -lcblas

all: CompareModels

%.o : %.cpp
	$(CXX) -c -o $@ $(CPPFLAGS) $<

CompareModels: CompareModels.o
	make -C imagelib
	make -C matrix
	$(CXX) -o $@ $(CPPFLAGS) $(LIB_PATH) CompareModels.o $(LIBS)

clean:
	make -C imagelib clean
	make -C matrix clean
	rm -f *.o *~ CompareModels
