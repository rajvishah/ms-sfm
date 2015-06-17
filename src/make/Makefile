CC=g++
CFLAGS=-O0 -w -c -g -Wall
#CFLAGS=-O3 -w -c -Wall

LIBPATH=-L../lib/ann_1.1_char/lib/ -L../lib/zlib/lib -L/home/cvit/rajvi/local_libs/lib/ -L/home/cvit/rajvi/bundler/lib/ -L/usr/lib64 -L/usr/lib64/atlas/ -L/global/OpenBLAS/lib/
IFLAGS=-I../lib/ann_1.1_char/include/ -I../lib/zlib/include/ -I/home/cvit/rajvi/local_libs/include/
LIBS=-lANN_char -lz -lmatrix -lcblas -llapack -lminpack -limage
default : make_tracks triangulate_tracks
	cp make_tracks triangulate_tracks ../bin/

triangulate_tracks: TriangulateTracks.o argvparser.o Reader.o Bundle.o
	$(CC) TriangulateTracks.o argvparser.o Reader.o Bundle.o $(LIBPATH) -Wall -o triangulate_tracks $(LIBS)

TriangulateTracks.o: TriangulateTracks.cpp argvparser.o
	$(CC) $(CFLAGS) TriangulateTracks.cpp

make_tracks: MakeTracks.o argvparser.o
	$(CC) MakeTracks.o argvparser.o -Wall -o make_tracks

MakeTracks.o : MakeTracks.cpp 
	$(CC) $(CFLAGS) MakeTracks.cpp 

Reader.o: Reader.cpp Reader.h
	$(CC) $(CFLAGS) $(IFLAGS) Reader.cpp

Bundle.o: Bundle.cpp Bundle.h
	$(CC) $(CFLAGS) $(IFLAGS) Bundle.cpp


argvparser.o: argvparser.cpp argvparser.h
	$(CC) $(CFLAGS) $(IFLAGS) argvparser.cpp
clean:
	rm -rf *o make_tracks
