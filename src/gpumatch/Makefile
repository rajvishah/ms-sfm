CC=/usr/local/cuda-6.5/bin/nvcc
CFLAGS=-O3 -w -c 
LDFLAGS=-L/home/vanshika/SiftGPU/bin/ -lsiftgpu
PKGCONFIGFLAG=`pkg-config --cflags --libs opencv`


match_pairs: match_image_pair.o sifts.o Gridder.o Matcher.o
	$(CC) $(IFLAGS) match_image_pair.o sifts.o Gridder.o Matcher.o $(LDFLAGS) $(PKGCONFIGFLAG) -o match_pairs

match_image_pair.o: match_image_pair.cu sifts.cpp sifts.h Gridder.cu Gridder.h  Matcher.cu Matcher.o
	$(CC) $(CFLAGS) match_image_pair.cu

sifts.o: sifts.cpp sifts.h
	$(CC) $(CFLAGS) $(IFLAGS) sifts.cpp

Gridder.o: Gridder.cu Gridder.h
	$(CC) $(CFLAGS) $(IFLAGS) Gridder.cu

Matcher.o: Matcher.cu Matcher.h
	$(CC) $(CFLAGS) $(IFLAGS) Matcher.cu
clean:
	rm -rf *o match_pairs
