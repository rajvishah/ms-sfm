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
    cmd.defineOption("track_file", "Path to base directory that stores all track files", 
            ArgvParser::OptionRequired);
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
    string trackFile = cmd.optionValue("track_file");

    FILE* inputFile = fopen(trackFile.c_str(), "r");
    if(inputFile == NULL) {
        printf("\nNo track file to process %s", trackFile.c_str());
        return 0;
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

            track.push_back(pt);
            count++;
        }
        double x,y,z;
        fscanf(inputFile, " %lf %lf %lf\n", &x, &y, &z);
        v3_t point;
        bool status = false;
        double reproErr = 0;
        bool angleVerify = true;
        status = triang::TriangulateTrack(&bdl, imList, track, point,angleVerify, &reproErr);
        printf("\n%lf", reproErr);
    }

    fclose(inputFile);
    return 0;
}

