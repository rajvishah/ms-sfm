CC=g++ -g
CFLAGS=-O0 -w -g -c -Wall
#CFLAGS=-O3 -w -c -Wall

# Change the path in the following to your ANN and zlib distribution paths
# Alternatively, copy the lib and .h files to your global lib and include paths

LIBPATH=-L/home/cvit/rajvi/bundler/lib/ann_1.1_char/lib/ -L/home/cvit/rajvi/bundler/lib/zlib/lib -L/home/cvit/rajvi/local_libs/lib/ -L/home/cvit/rajvi/bundler/lib/ -L/home/cvit/rajvi/bundler/lib/sfm-driver/  -L/usr/lib64 -L/usr/lib64/atlas/ -L/global/OpenBLAS/lib/ 

IFLAGS=-I/home/cvit/rajvi/bundler/lib/ann_1.1_char/include/ -I../lib/zlib/include/ -I/home/cvit/rajvi/local_libs/include/ -I/home/cvit/rajvi/bundler/lib/sfm-driver/ -I/home/cvit/rajvi/bundler/lib/imagelib/ 

LIBS=-lsfmdrv -limage -lsba.v1.5 -lANN_char -lz -lmatrix -lcblas -llapack -lminpack -ljpeg 

PKGCONFIGFLAG=`pkg-config --cflags --libs opencv`

default: localizer

localizer: Match3Dto2D.o Localize.o ImageData.o Camera.o ImageSize.o keys2a.o Reader.o Bundle.o
	$(CC) $(IFLAGS) Match3Dto2D.o Localize.o ImageData.o Camera.o ImageSize.o keys2a.o Reader.o Bundle.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o localizer $(LIBS)

Match3Dto2D.o:Match3Dto2D.cpp
	$(CC) $(CFLAGS) $(IFLAGS) Match3Dto2D.cpp

Localize.o: ../base/Localize.cpp ../base/Localize.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/Localize.cpp

Reader.o: ../base/Reader.cpp ../base/Reader.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/Reader.cpp

Bundle.o: ../base/Bundle.cpp ../base/Bundle.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/Bundle.cpp

keys2a.o: ../base/keys2a.cpp ../base/keys2a.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/keys2a.cpp

ImageSize.o: ../base/ImageSize.cpp ../base/ImageSize.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/ImageSize.cpp

ImageData.o: ../base/ImageData.cpp ../base/ImageData.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/ImageData.cpp

Camera.o: ../base/Camera.cpp ../base/Camera.h
	$(CC) $(CFLAGS) $(IFLAGS) ../base/Camera.cpp

clean:
	rm -rf *o localizer
