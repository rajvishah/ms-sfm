CC=g++ 
#CFLAGS=-O0 -w -g -c -Wall
CFLAGS=-O3 -w -c -Wall

# Change the path in the following to your ANN and zlib distribution paths
# Alternatively, copy the lib and .h files to your global lib and include paths

LIBPATH=-L/home/cvit/rajvi/bundler/lib/ann_1.1_char/lib/ -L/home/cvit/rajvi/bundler/lib/zlib/lib -L/home/cvit/rajvi/local_libs/lib/ -L/home/cvit/rajvi/bundler/lib/ -L/home/cvit/rajvi/bundler/lib/sfm-driver/  -L/usr/lib64 -L/usr/lib64/atlas/ -L/global/OpenBLAS/lib/ -L/global/boost/1.55.0/lib 

IFLAGS=-I/home/cvit/rajvi/bundler/lib/ann_1.1_char/include/ -I../lib/zlib/include/ -I/home/cvit/rajvi/local_libs/include/ -I/home/cvit/rajvi/bundler/lib/sfm-driver/ -I/home/cvit/rajvi/bundler/lib/imagelib/ -I /global/boost/1.55.0/include/ 

LIBS=-lsfmdrv -limage -lsba.v1.5 -lANN_char -lz -lmatrix -lcblas -llapack -lminpack -ljpeg -lboost_iostreams -lboost_serialization 

PKGCONFIGFLAG=`pkg-config --cflags --libs opencv`

default: localizer CreateGuidedMatchPairs GuidedMatchLists CreateGlobalMatchPairs GlobalMatchLists make_tracks triangulate_tracks merge_cameras merge_tracks KeyMatchFull 
	mv -t ../../bin localizer CreateGuidedMatchPairs GuidedMatchLists make_tracks triangulate_tracks merge_cameras merge_tracks CreateGlobalMatchPairs GlobalMatchLists KeyMatchFull 
localizer: Match3Dto2D.o ../base/Localize.o ../base/ImageData.o ../base/Camera.o ../base/ImageSize.o ../base/keys2a.o ../base/Reader.o ../base/Bundle.o
	$(CC) $(IFLAGS) Match3Dto2D.o ../base/Localize.o ../base/ImageData.o ../base/Camera.o ../base/ImageSize.o ../base/keys2a.o ../base/Reader.o ../base/Bundle.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o localizer $(LIBS)


CreateGlobalMatchPairs: CreateGlobalMatchPairs.o ../base/keys2a.o ../base/argvparser.o ../base/MatchPairs.o ../base/Reader.o
	$(CC) $(IFLAGS) CreateGlobalMatchPairs.o ../base/keys2a.o ../base/argvparser.o ../base/Reader.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o CreateGlobalMatchPairs $(LIBS)

KeyMatchFull: KeyMatchFull.o ../base/keys2a.o ../base/argvparser.o ../base/Reader.o
	$(CC) $(IFLAGS) KeyMatchFull.o ../base/keys2a.o ../base/argvparser.o ../base/Reader.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o KeyMatchFull $(LIBS)

GlobalMatchLists: GlobalMatchLists.o ../base/keys2a.o ../base/argvparser.o ../base/Reader.o
	$(CC) $(IFLAGS) GlobalMatchLists.o ../base/keys2a.o ../base/argvparser.o ../base/Reader.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o GlobalMatchLists $(LIBS)
	
CreateGlobalMatchPairs.o: CreateGlobalMatchPairs.cpp ../base/argvparser.cpp ../base/argvparser.h
	$(CC) $(CFLAGS) $(IFLAGS) CreateGlobalMatchPairs.cpp

GlobalMatchLists.o: GlobalMatchLists.cpp ../base/argvparser.cpp ../base/argvparser.h ../base/keys2a.cpp ../base/keys2a.h
	$(CC) $(CFLAGS) $(IFLAGS) GlobalMatchLists.cpp

KeyMatchFull.o: KeyMatchFull.cpp ../base/argvparser.cpp ../base/argvparser.h ../base/keys2a.cpp ../base/keys2a.h
	$(CC) $(CFLAGS) $(IFLAGS) KeyMatchFull.cpp

CreateGuidedMatchPairs: CreateGuidedMatchPairs.o ../base/keys2a.o ../base/Geometric.o ../base/Matcher.o ../base/Gridder.o ../base/argvparser.o ../base/MatchPairs.o ../base/Reader.o ../base/Bundle.o
	$(CC) $(IFLAGS) CreateGuidedMatchPairs.o ../base/keys2a.o ../base/Geometric.o ../base/Matcher.o ../base/Gridder.o ../base/argvparser.o ../base/MatchPairs.o ../base/Reader.o ../base/Bundle.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o CreateGuidedMatchPairs $(LIBS)

GuidedMatchLists: GuidedMatchLists.o ../base/keys2a.o ../base/Geometric.o ../base/Matcher.o ../base/Gridder.o ../base/argvparser.o ../base/MatchPairs.o ../base/Reader.o ../base/Bundle.o
	$(CC) $(IFLAGS) GuidedMatchLists.o ../base/keys2a.o ../base/Geometric.o ../base/Matcher.o ../base/Gridder.o ../base/argvparser.o ../base/MatchPairs.o ../base/Reader.o ../base/Bundle.o $(LIBPATH) $(PKGCONFIGFLAG)-Wall -o GuidedMatchLists $(LIBS)
	
CreateGuidedMatchPairs.o: CreateGuidedMatchPairs.cpp ../base/Reader.cpp ../base/Reader.h ../base/Bundle.cpp ../base/Bundle.h ../base/Gridder.cpp ../base/Gridder.h ../base/defs.h ../base/Geometric.cpp ../base/Geometric.h ../base/Matcher.cpp ../base/Matcher.h ../base/argvparser.cpp ../base/argvparser.h ../base/MatchPairs.cpp ../base/MatchPairs.h
	$(CC) $(CFLAGS) $(IFLAGS) CreateGuidedMatchPairs.cpp

GuidedMatchLists.o: GuidedMatchLists.cpp ../base/Reader.cpp ../base/Reader.h ../base/Bundle.cpp ../base/Bundle.h ../base/Gridder.cpp ../base/Gridder.h ../base/defs.h ../base/Geometric.cpp ../base/Geometric.h ../base/Matcher.cpp ../base/Matcher.h ../base/argvparser.cpp ../base/argvparser.h ../base/MatchPairs.cpp ../base/MatchPairs.h
	$(CC) $(CFLAGS) $(IFLAGS) GuidedMatchLists.cpp

triangulate_tracks: TriangulateTracks.o ../base/argvparser.o ../base/Reader.o ../base/Bundle.o
	$(CC) TriangulateTracks.o ../base/argvparser.o ../base/Reader.o ../base/Bundle.o $(LIBPATH) -Wall -o triangulate_tracks $(LIBS)

make_tracks: MakeTracks.o ../base/argvparser.o
	$(CC) MakeTracks.o ../base/argvparser.o -Wall -o make_tracks

merge_cameras: MergeCameras.o
	$(CC) MergeCameras.o -Wall -o merge_cameras

merge_tracks: MergeTracks.o
	$(CC) MergeTracks.o $(LIBPATH) -Wall -o merge_tracks $(LIBS) 

MergeTracks.o:MergeTracks.cpp
	$(CC) $(CFLAGS) $(IFLAGS) MergeTracks.cpp

MergeCameras.o:MergeCameras.cpp
	$(CC) $(CFLAGS) $(IFLAGS) MergeCameras.cpp

Match3Dto2D.o:Match3Dto2D.cpp
	$(CC) $(CFLAGS) $(IFLAGS) Match3Dto2D.cpp

MakeTracks.o : MakeTracks.cpp 
	$(CC) $(CFLAGS) $(IFLAGS) MakeTracks.cpp 

TriangulateTracks.o: TriangulateTracks.cpp 
	$(CC) $(CFLAGS) $(IFLAGS) TriangulateTracks.cpp
clean:
	rm -rf localizer CreateGuidedMatchPairs GuidedMatchLists CreateGlobalMatchPairs GlobalMatchPairs make_tracks triangulate_tracks merge_cameras *.o
