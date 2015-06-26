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


int main(int argc, char* argv[]) {
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string trackDir = cmd.optionValue("tracks_dir");
    string imageIdxStr = cmd.optionValue("image_idx");
    int imageIdx = atoi( imageIdxStr.c_str() );

    char file1[1000], file2[1000];
    sprintf(file1, "%s/sorted-tracks-%d.txt", trackDir.c_str(), imageIdx);
    sprintf(file2, "%s/long-tracks-%d.txt", trackDir.c_str(), imageIdx);

    FILE* inputFile = fopen(file1, "r");
    if(inputFile == NULL) {
        printf("\nNo track file to do anything");
        return 0;
    }

    FILE* outputFile = fopen(file2, "w");
    if(outputFile == NULL) {
        printf("\nOutput file could not be created");
        return -1;
    }

    int imIdx = -1;

    int siftId = -1, prevSiftId = -1, numTracks = 0;
    TrackStruct trk;
    while( fscanf(inputFile, "%d", &imIdx)!= EOF) {    
        fscanf(inputFile, "%d", &siftId);
        if(siftId != prevSiftId) {
            if(!trk.views.empty()) {
                numTracks++;
//                printf("\nWriting track %d", numTracks);
                fprintf(outputFile, "%d", trk.views.size());
                for(int i=0; i < trk.views.size(); i++) {
                    fprintf(outputFile, " %d", trk.views[i]);
                    fprintf(outputFile, " %d", trk.sifts[i]);
                    fprintf(outputFile, " %lf", trk.x[i]);
                    fprintf(outputFile, " %lf", trk.y[i]);
                }
                fprintf(outputFile, "\n");
                trk.views.clear();
                trk.sifts.clear();
                trk.x.clear();
                trk.y.clear();
            }
        } 
     
        double x1, y1, x2, y2;
        int view2, siftId2;
        fscanf(inputFile, "%lf %lf %d %d %lf %lf", 
                &x1, &y1, &view2, &siftId2, &x2, &y2);
    
        if(trk.views.empty()) {
            trk.views.push_back(imIdx);
            trk.sifts.push_back(siftId);
            trk.x.push_back(x1);
            trk.y.push_back(y1);
        }
        trk.views.push_back(view2);
        trk.sifts.push_back(siftId2);
        trk.x.push_back(x2);
        trk.y.push_back(y2);
        prevSiftId = siftId;
    }

    fclose(outputFile);
    printf("\nFinished writing %d tracks for %d image", numTracks, imageIdx);
    return 0;
}
