#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <time.h>
#include <omp.h>

#include "../base/Bundle.h"
#include "../base/Reader.h"
#include "../base/argvparser.h"
#include "../base/Triangulater.h"
using namespace std;
using namespace CommandLineProcessing;

bool ReadTracksFromFile(string filename, vector< vector <pointstr> >& tracksVector) {
  clock_t start1 = clock();   
  int trackLength;
  FILE* inputFile = fopen(filename.c_str(), "r");
  if(inputFile == NULL) {
    printf("\nNo input file to read tracks from %s", filename.c_str());
    return false;
  }
  while( fscanf(inputFile, "%d", &trackLength)!= EOF) {  
    int count = 0; 
    vector< pointstr > track;
    while(count < trackLength) {
      pointStruct pt;
      fscanf(inputFile, "%d", &pt.viewId);
      fscanf(inputFile, "%d", &pt.siftId);
      fscanf(inputFile, "%lf", &pt.xcord);
      fscanf(inputFile, "%lf", &pt.ycord);
      track.push_back(pt);
      count++;
    }
    tracksVector.push_back(track);
  }
  fclose(inputFile);
  clock_t end1 = clock();    
  printf("\n[ TriangulateTracks - timings ] Reading tracks took %0.6fs\n", 
      (end1 - start1) / ((double) CLOCKS_PER_SEC));
}

bool MakeTracksFromMatches(string filename, vector< vector <pointstr> >& tracksVector, int numImages) {
  ifstream inputFile(filename.c_str(), ifstream::in);
  if(!inputFile.is_open()) {
    printf("\nNo match file for further processing");
    return false;
  }
  
  printf("\nReading matches Files");
  vector < vector< pair< pointstr, pointstr> > > matchPairLists(numImages);
  vector < vector< pair< int, int > > > keyToListMaps(numImages);

  clock_t start1 = clock();   

  string line;
  while(getline(inputFile,line)) {
    int imId1, imId2, siftId1, siftId2;
    double x1, y1, x2, y2;
    stringstream lineStream(line);

    lineStream >> imId1 >> siftId1 >> x1 >> y1;
    lineStream >> imId2 >> siftId2 >> x2 >> y2;

    unsigned long long int key;
    pointstr p1, p2;

    p1.viewId = imId1;
    p1.siftId = siftId1;
    p1.xcord = x1;
    p1.ycord = y1;

    p2.viewId = imId2;
    p2.siftId = siftId2;
    p2.xcord = x2;
    p2.ycord = y2;

    matchPairLists[imId1].push_back( make_pair(p1, p2) );
    matchPairLists[imId2].push_back( make_pair(p2, p1) );

    int listId1 = matchPairLists[imId1].size() - 1;
    int listId2 = matchPairLists[imId2].size() - 1;

    keyToListMaps[imId1].push_back( make_pair(siftId1, listId1) ); 
    keyToListMaps[imId2].push_back( make_pair(siftId2, listId2) ); 
  }
  clock_t end1 = clock();    
  printf("\n[ TriangulateTracks - timings ] Reading file took %0.6fs\n", 
      (end1 - start1) / ((double) CLOCKS_PER_SEC));

  clock_t start2 = clock();
  for(int i=0; i < keyToListMaps.size(); i++) {
    sort( keyToListMaps[i].begin(), keyToListMaps[i].end() );

    int counter = 0;
    while(counter < keyToListMaps[i].size() ) {
      int currKey = keyToListMaps[i][counter].first;
      int listIdx = keyToListMaps[i][counter].second;

      pointstr p1 = matchPairLists[i][listIdx].first;
      pointstr p2 = matchPairLists[i][listIdx].second;

      vector< pointstr > currTrack;
      currTrack.push_back( p1 );
      currTrack.push_back( p2 );

      int incr = 1;
      while(keyToListMaps[i][counter+incr].first == currKey) {
        listIdx = keyToListMaps[i][counter+incr].second;
        pointstr p2 = matchPairLists[i][listIdx].second;
        currTrack.push_back( p2 );
        incr++;
      }
      tracksVector.push_back(currTrack);
      counter = counter + incr;
    }
  }
  clock_t end2 = clock();    
  int numProcs = omp_get_num_procs();
  printf("\n[ TriangulateTracks - timings ]  Sorting and finding tracks took %0.6fs\n", 
      (end2 - start2) / ((double) CLOCKS_PER_SEC));
  printf("\n[ TriangulateTracks - timings ]  Sorting and finding tracks with openmp support would take %0.6fs\n", 
      (end2 - start2) / ((double) CLOCKS_PER_SEC * numProcs));

  return true;
}


bool WriteTriangulatedTracks(string filename, vector< vector < pointstr > >& tracksVec, 
    vector< bool > &triStatus, vector< v3_t > &triPoints, string mode) {

  clock_t start = clock();
  FILE* outputFile = fopen(filename.c_str(), "w");
  if(outputFile == NULL) {
    printf("\nOutput file could not be created");
    fflush(stdout);
    return -1;
  }

  for(int trackId = 0; trackId < tracksVec.size(); trackId++) {
    if(triStatus[trackId]) {
      v3_t point = triPoints[trackId];
      vector< pointstr > &track = tracksVec[trackId];
      if(mode == "bundle") {
        fprintf(outputFile,"%lf %lf %lf\n", 
            point.p[0],  point.p[1], point.p[2]);
        fprintf(outputFile, "0 0 0\n");
        fprintf(outputFile, "%d", track.size());
        for(int i=0; i < track.size(); i++) {
          fprintf(outputFile, " %d", track[i].viewId);
          fprintf(outputFile, " %d", track[i].siftId);
          fprintf(outputFile, " %lf", track[i].xcord);
          fprintf(outputFile, " %lf", track[i].ycord);    
        }
        fprintf(outputFile,"\n");
      } else {
        fprintf(outputFile, "%d", track.size());
        for(int i=0; i < track.size(); i++) {
          fprintf(outputFile, " %d", track[i].viewId);
          fprintf(outputFile, " %d", track[i].siftId);
          fprintf(outputFile, " %lf", track[i].xcord);
          fprintf(outputFile, " %lf", track[i].ycord);    
        }
        fprintf(outputFile," %lf %lf %lf\n", 
            point.p[0],  point.p[1], point.p[2]);
      }
    }
  } 
  fclose(outputFile);
  clock_t end = clock();    

  printf("[TriangulateTracks] Writing triangulated file mode %s took %0.6fs\n", 
      (end - start) / ((double) CLOCKS_PER_SEC), mode.c_str());
}


void TriangulateAllTracks(bundle::Bundle* bdl, reader::ImageListReader& imList, 
    vector< vector<pointstr> > &tracksVec, vector<bool> &triStatus, 
    vector< v3_t> &triPoints, bool angleVerify) {

  clock_t start3 = clock();
  //#pragma omp parallel for
  for(int trackId = 0; trackId < tracksVec.size(); trackId++) {
    if(trackId % 10000 == 0)
      printf("\nTriangulating track %d / %d", trackId, tracksVec.size());
    vector < pointstr > &track = tracksVec[trackId];
    v3_t point;
    double reproErr = 0.0;
    triStatus[trackId] = triang::TriangulateTrack(bdl, imList, 
        track, point, angleVerify, &reproErr);
    triPoints[trackId] = point;
  }
  int numProcs = omp_get_num_procs();
  clock_t end3 = clock();    
  printf("\n[ TriangulateTracks - timings ]  Triangulating tracks took %0.6fs\n", 
      (end3 - start3) / ((double) CLOCKS_PER_SEC));
  printf("\n[ TriangulateTracks - timings ]  Triangulating tracks with omp support would take %0.6fs\n", 
      (end3 - start3) / ((double) CLOCKS_PER_SEC * numProcs) );
}




void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
    cmd.setIntroductoryDescription("Track formation and triangulation from matches");

    //define error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help",""); 
    cmd.defineOption("lists_path", "Lists Path", ArgvParser::OptionRequired);
    cmd.defineOption("bundler_path", "Bundler Path", ArgvParser::OptionRequired);
    cmd.defineOption("input_path", "Input Path", ArgvParser::OptionRequired);
    cmd.defineOption("output_path", "Output Path", ArgvParser::OptionRequired);
    cmd.defineOption("mode", "Mode", ArgvParser::NoOptionAttribute);
    cmd.defineOption("num_images", "number of images", ArgvParser::OptionRequired); 
    /// finally parse and handle return codes (display help etc...)
    int result = cmd.parse(argc, argv);
    if (result != ArgvParser::NoParserError)
    {
        cout << cmd.parseErrorDescription(result);
        exit(-1);
    }
}



int main(int argc, char* argv[]) {
  /*
   * This program can be used in 
   *
   *
   *
   */
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string listsPath = cmd.optionValue("lists_path");
    string bundlerPath = cmd.optionValue("bundler_path");
    string inputPath = cmd.optionValue("input_path");
    string outputPath = cmd.optionValue("output_path");
    string numImagesStr = cmd.optionValue("num_images");

    string mode = "";
    if(cmd.foundOption("mode")) {
        mode = cmd.optionValue("mode");
    }

    int numImages = atoi( numImagesStr.c_str() );
    
    bundle::Bundle bdl;
    reader::ImageListReader imList( listsPath );
    reader::BundleReader br(bundlerPath, &bdl); 

    bool status = imList.read();
    if(!status) {
        cout << "\nProblem reading image list " << listsPath;
        return -1;
    }

    status = br.read();
    if(!status) {
        cout << "\nProblem reading bundle file";
        return -1;
    }

    printf("\nSuccessfully Read Bundle and List Files");

    FILE* outputFile = NULL;
    FILE* outputFile1 = NULL;
    char file1[1000], file2[1000], file3[1000];

    bool angleVerify = false;
    vector < vector < pointstr > > tracksVec;  
    int initialTrackCount = 0, finalTrackCount = 0;
    int numProcs = omp_get_num_procs();
    //omp_set_num_threads(numProcs);
    if(mode == "merged") {
        angleVerify = true;
        sprintf(file1, "%s/merged-tracks.txt", inputPath.c_str());
        sprintf(file2, "%s/triangulated-tracks-final.txt", 
                outputPath.c_str());
        sprintf(file3, "%s/bundle-triangulated_tracks-final.txt", 
                outputPath.c_str());
        
        outputFile1 = fopen(file3, "w");
        if(outputFile1 == NULL) {
            printf("\nOutput1 file could not be created");
            return -1;
        }

        printf("\nReading merged tracks Files");
        if(!ReadTracksFromFile(string(file1), tracksVec)) return -1;
    } else {
        sprintf(file1, "%s/matches.txt", inputPath.c_str());
        sprintf(file2, "%s/triangulated-tracks.txt", outputPath.c_str());
        
        if(!MakeTracksFromMatches(string(file1), tracksVec, numImages)) return -1;
    }

    vector<bool> triStatus( tracksVec.size() );
    vector<v3_t> triPoints( tracksVec.size() );
    TriangulateAllTracks(&bdl, imList, tracksVec, triStatus, triPoints, angleVerify);

//    bool status1 = WriteTriangulatedTracks(file2, tracksVec, triStatus, triPoints, "bundle");
    bool status2 = WriteTriangulatedTracks(string(file2), tracksVec, triStatus, triPoints, "normal");
    printf("Done writing triangulated tracks for all images, Initial  %d, Final %d\n",
            initialTrackCount, finalTrackCount);
    return 0;
}

