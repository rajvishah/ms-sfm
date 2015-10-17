#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <cstring>
#include <queue>
#include <stdlib.h>
#include <time.h>

#include "../base/Bundle.h"
#include "../base/Reader.h"
#include "../base/Triangulater.h"
#include "../base/argvparser.h"

using namespace std;
using namespace CommandLineProcessing;

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
    cmd.setIntroductoryDescription("Track formation and triangulation from matches");

    //define error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help",""); 
    cmd.defineOption("lists_path", "Lists Path", ArgvParser::OptionRequired);
    cmd.defineOption("bundler_path", "Bundler Path", ArgvParser::OptionRequired);
    cmd.defineOption("mode", "Mode", ArgvParser::NoOptionAttribute);
    cmd.defineOption("tracks_dir", "Path to base directory that stores all track files", 
            ArgvParser::OptionRequired);
    cmd.defineOption("image_idx", "image_index", ArgvParser::OptionRequired); 
    /// finally parse and handle return codes (display help etc...)
    int result = cmd.parse(argc, argv);
    if (result != ArgvParser::NoParserError)
    {
        cout << cmd.parseErrorDescription(result);
        exit(-1);
    }
}

int main(int argc, char* argv[]) { 
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string listsPath = cmd.optionValue("lists_path");
    string bundlerPath = cmd.optionValue("bundler_path");
    string trackDir = cmd.optionValue("tracks_dir");
    string imageIdxStr = cmd.optionValue("image_idx");

    string mode = "";
    if(cmd.foundOption("mode")) {
        mode = cmd.optionValue("mode");
    }

    int imageIdx = atoi( imageIdxStr.c_str() );

    FILE* outputFile1 = NULL;
    FILE* outputFile2 = NULL;
    char file1[1000], file2[1000], file3[1000], file4[1000];

    bool angleVerify = false;
    if(mode == "merged") {
        angleVerify = true;
        sprintf(file1, "%s/merged-tracks.txt", trackDir.c_str());
        sprintf(file2, "%s/triangulated-tracks-final.txt", 
                trackDir.c_str());
        sprintf(file3, "%s/bundle-triangulated_tracks-final.txt", 
                trackDir.c_str());
        sprintf(file4, "%s/reprojection-errors-tracks-final.txt", 
                trackDir.c_str());
        
        outputFile1 = fopen(file3, "w");
        if(outputFile1 == NULL) {
            printf("\nOutput1 file could not be created");
            return -1;
        }
        outputFile2 = fopen(file4, "w");
        if(outputFile2 == NULL) {
            printf("\nOutput1 file could not be created");
            return -1;
        }
    } else {
        sprintf(file1, "%s/long-tracks-%d.txt", trackDir.c_str(), imageIdx);
        sprintf(file2, "%s/triangulated-tracks-%d.txt", 
                trackDir.c_str(), imageIdx);
        sprintf(file3, "%s/dbg_triangulated_tracks-%d.txt", 
                trackDir.c_str(), imageIdx);
    }

    FILE* inputFile = fopen(file1, "r");
    if(inputFile == NULL) {
        printf("\nNo track file to process %s", file1);
        return 0;
    }

    FILE* outputFile = fopen(file2, "w");
    if(outputFile == NULL) {
        printf("\nOutput file could not be created");
        return -1;
    }

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

    int trackLength;
    int initialTrackCount = 0, finalTrackCount = 0;
    
    long long int totalClock = 0;

    while( fscanf(inputFile, "%d", &trackLength)!= EOF) {  
        int count = 0; 
        vector< pointstr > track;
        while(count < trackLength) {
            pointStruct pt;
            fscanf(inputFile, "%d", &pt.viewId);
            fscanf(inputFile, "%d", &pt.siftId);
            fscanf(inputFile, "%lf", &pt.xcord);
            fscanf(inputFile, "%lf", &pt.ycord);

            if(bdl.validTriangulated[pt.viewId] == true) {
              track.push_back(pt);
            }
            count++;
        }

        v3_t point;
        clock_t start = clock();
        bool status = false;
        double reproErr = 0;
        if(track.size() > 1) {
          initialTrackCount++;
          status = triang::TriangulateTrack(&bdl, imList, track, point,angleVerify, &reproErr);
        }
        clock_t end = clock();

        totalClock += (end - start);
        if(status) {
            finalTrackCount++;
            if(mode == "merged") {
                fprintf(outputFile1,"%lf %lf %lf\n", 
                        point.p[0],  point.p[1], point.p[2]);
                fprintf(outputFile1, "0 0 0\n");
                fprintf(outputFile1, "%d", track.size());
                for(int i=0; i < track.size(); i++) {
                    fprintf(outputFile1, " %d", track[i].viewId);
                    fprintf(outputFile1, " %d", track[i].siftId);
                    fprintf(outputFile1, " %lf", track[i].xcord);
                    fprintf(outputFile1, " %lf", track[i].ycord);    
                }
                fprintf(outputFile1,"\n");
                fprintf(outputFile2,"%lf\n", reproErr);
            }
            fprintf(outputFile, "%d", track.size());
            for(int i=0; i < track.size(); i++) {
                fprintf(outputFile, " %d", track[i].viewId);
                fprintf(outputFile, " %d", track[i].siftId);
                fprintf(outputFile, " %lf", track[i].xcord);
                fprintf(outputFile, " %lf", track[i].ycord);    
            }
            fprintf(outputFile," %lf %lf %lf\n", 
                    point.p[0],  point.p[1], point.p[2]);
//            fprintf(outputFile1,"\n");
        }
        //printf("%d...", finalTrackCount);
    }
    fclose(inputFile);
    fclose(outputFile);
    if(outputFile1)
        fclose(outputFile1);
    printf("[TriangulateTracks] Triangulation took %0.6fs\n", 
            totalClock / ((double) CLOCKS_PER_SEC));
    printf("Done writing triangulated tracks for image %d, Initial  %d, Final %d\n",
            imageIdx, initialTrackCount, finalTrackCount);
    return 0;
}

