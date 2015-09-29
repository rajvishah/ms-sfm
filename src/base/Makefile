CC=g++ 
#CFLAGS=-O0 -g -w -c -Wall
CFLAGS=-O3 -w -c -Wall

# Change the path in the following to your ANN and zlib distribution paths
# Alternatively, copy the lib and .h files to your global lib and include paths

BUNDLER_PATH=/home/rajvi/Bundler/

LIBPATH=-L$(BUNDLER_PATH)/lib/ann_1.1_char/lib/ -L$(BUNDLER_PATH)/lib/zlib/lib -L/home/rajvi/local_libs/lib/ -L$(BUNDLER_PATH)/lib/ -L$(BUNDLER_PATH)/lib/sfm-driver/  -L/usr/lib64 -L/usr/lib64/atlas/ -L/global/OpenBLAS/lib/ 

IFLAGS=-I$(BUNDLER_PATH)/lib/ann_1.1_char/include/ -I../lib/zlib/include/ -I/home/rajvi/local_libs/include/ -I$(BUNDLER_PATH)/lib/sfm-driver/ -I$(BUNDLER_PATH)/lib/imagelib/ -I$(BUNDLER_PATH)/lib/matrix/

LIBS=-lsfmdrv -limage -lsba.v1.5 -lANN_char -lz -lmatrix -lcblas -llapack -lminpack -ljpeg 

PKGCONFIGFLAG=`pkg-config --cflags --libs opencv`

default: Triangulater.o Localize.o Reader.o Bundle.o keys2a.o ImageSize.o ImageData.o Camera.o Gridder.o Matcher.o Geometric.o argvparser.o MatchPairs.o 

Localize.o: Localize.cpp Localize.h
	$(CC) $(CFLAGS) $(IFLAGS) Localize.cpp

Triangulater.o: Triangulater.cpp Triangulater.h
	$(CC) $(CFLAGS) $(IFLAGS) Triangulater.cpp

Reader.o: Reader.cpp Reader.h
	$(CC) $(CFLAGS) $(IFLAGS) Reader.cpp

Bundle.o: Bundle.cpp Bundle.h
	$(CC) $(CFLAGS) $(IFLAGS) Bundle.cpp

keys2a.o: keys2a.cpp keys2a.h
	$(CC) $(CFLAGS) $(IFLAGS) keys2a.cpp

ImageSize.o: ImageSize.cpp ImageSize.h
	$(CC) $(CFLAGS) $(IFLAGS) ImageSize.cpp

ImageData.o: ImageData.cpp ImageData.h
	$(CC) $(CFLAGS) $(IFLAGS) ImageData.cpp

Camera.o: Camera.cpp Camera.h
	$(CC) $(CFLAGS) $(IFLAGS) Camera.cpp

Gridder.o: Gridder.cpp Gridder.h
	$(CC) $(CFLAGS) $(IFLAGS) Gridder.cpp

Matcher.o: Matcher.cpp Matcher.h
	$(CC) $(CFLAGS) $(IFLAGS) Matcher.cpp

Geometric.o: Geometric.cpp Geometric.h
	$(CC) $(CFLAGS) $(IFLAGS) Geometric.cpp

MatchPairs.o: MatchPairs.cpp MatchPairs.h
	$(CC) $(CFLAGS) $(IFLAGS) MatchPairs.cpp

argvparser.o: argvparser.cpp argvparser.h
	$(CC) $(CFLAGS) $(IFLAGS) argvparser.cpp

clean:
	rm -rf *o
