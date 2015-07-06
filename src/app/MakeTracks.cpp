#include <sstream>
#include "../base/defs.h"
#include "../base/argvparser.h"
#include "../base/Reader.h"
#include "../base/Bundle.h"

using namespace CommandLineProcessing;

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
    cmd.setIntroductoryDescription("Track formation and triangulation from matches");

    //define error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help",""); 
    cmd.defineOption("tracks_dir", "Path to base directory that stores all track files", 
            ArgvParser::OptionRequired);
    cmd.defineOption("matches_file", "Path to matches file", 
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

typedef struct TrackStruct {
    vector< int > views;
    vector< int > sifts;
    vector< double > x;
    vector< double > y;
};

typedef struct pointStruct { 
    int viewId;
    int siftId;
    double xcord;
    double ycord;
}pointstr;

int main(int argc, char* argv[]) {
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string trackDir = cmd.optionValue("tracks_dir");
    string matchesFilePath = cmd.optionValue("matches_file");
    string imageIdxStr = cmd.optionValue("image_idx");
    int imageIdx = atoi( imageIdxStr.c_str() );

    char file2[1000];
    sprintf(file2, "%s/long-tracks-%d.txt", trackDir.c_str(), imageIdx);

    ifstream inputFile( matchesFilePath.c_str(), ifstream::in);
    if(!inputFile.is_open()) {
        printf("\nNo track file to do anything");
        return 0;
    }



    FILE* outputFile = fopen(file2, "w");
    if(outputFile == NULL) {
        printf("\nOutput file could not be created");
        return -1;
    }

    vector< pair< pointstr, pointstr> > matchPairList;
    vector< pair< int, int > > keyToListMap;
    /*
    FILE* inputFile = fopen(matchesFilePath.c_str(), "rb");
    if(inputFile == NULL) {
        printf("\nInput file could not be opened");
        return -1;
    }
    fseek(inputFile, 0, SEEK_END);
    long long fileSize = ftell(inputFile);
    rewind(inputFile);

    long int unitSize = sizeof(pair< pointstr, pointstr> );
    int numUnits = fileSize/unitSize;
    

    for(int i=0; i < numUnits; i++) {
        pair< pointstr, pointstr > matchPair;
        fread(matchPair.data(), unitSize, 1, inputFile);
        
        pointstr p1 = matchPair.first;
        pointstr p2 = matchPair.second;

        if(p1.siftId == imageIdx) {
            matchPairList.push_back(make_pair(p1,p2));
            int listId = matchPairList.size();
            keyToListMap.push_back( make_pair(p1.siftId, listId) ); 
        } else if(p2.siftId == imageIdx) {
            matchPairList.push_back(make_pair(p2,p1)); 
            int listId = matchPairList.size();
            keyToListMap.push_back( make_pair(p2.siftId, listId) ); 
        }
    }*/


    string line;
    while(getline(inputFile,line)) {
        int imId1, imId2, siftId1, siftId2;
        double x1, y1, x2, y2;
        istringstream lineStream(line);

        lineStream >> imId1 >> siftId1 >> x1 >> y1;
        lineStream >> imId2 >> siftId2 >> x2 >> y2;
        
        unsigned long long int key;
        pointstr p1, p2;
        if(imId1 == imageIdx || imId2 == imageIdx) {
            p1.viewId = imId1;
            p1.siftId = siftId1;
            p1.xcord = x1;
            p1.ycord = y1;
            
            p2.viewId = imId2;
            p2.siftId = siftId2;
            p2.xcord = x2;
            p2.ycord = y2;
            
            if(imId2 == imageIdx) {
                matchPairList.push_back( make_pair(p2, p1) );
                int listId = matchPairList.size();
                keyToListMap.push_back( make_pair(siftId2, listId) ); 
            } else {
                matchPairList.push_back( make_pair(p1, p2) ); 
                int listId = matchPairList.size();
                keyToListMap.push_back( make_pair(siftId1, listId) ); 
            }
        } 
    }

    sort( keyToListMap.begin(), keyToListMap.end() );

    vector< vector< pointstr> > tracks;  
    int counter = 0;
    while(counter < keyToListMap.size() ) {
        int currKey = keyToListMap[counter].first;
        int listIdx = keyToListMap[counter].second;
        
        pointstr p1 = matchPairList[listIdx].first;
        pointstr p2 = matchPairList[listIdx].second;

        vector< pointstr > currTrack;
        currTrack.push_back( p1 );
        currTrack.push_back( p2 );
        
        int incr = 1;
        while(keyToListMap[counter+incr].first == currKey) {
            listIdx = keyToListMap[counter+incr].second;
            pointstr p2 = matchPairList[listIdx].second;
            currTrack.push_back( p2 );
            incr++;
        }
        tracks.push_back(currTrack);
        counter = counter + incr;
    }
    printf("\nFound %d tracks", tracks.size());

    for(int i=0; i < tracks.size(); i++) {
        fprintf(outputFile, "%d", tracks[i].size());
        for(int j=0; j < tracks[i].size(); j++) {
            pointstr p = tracks[i][j];
            fprintf(outputFile, " %d %d %lf %lf", p.viewId, p.siftId, p.xcord, p.ycord); 
        }
        fprintf(outputFile, "\n");
    }

    fclose(outputFile);
    printf("\nFinished writing %d tracks for %d image", tracks.size(), imageIdx);
    return 0;
}
