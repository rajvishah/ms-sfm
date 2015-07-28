#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../base/Reader.h"
#include "../base/Bundle.h"
#include "../base/keys2a.h"

using namespace std;

typedef struct pointStruct { 
    int viewId;
    int siftId;
    double xcord;
    double ycord;
}pointstr;


int main( int argc, char* argv[]) {
    string bundleFilePath( argv[1] );
    string camFilesPath( argv[2] );
    string bundleOutPath( argv[3] );

    ifstream bundleFile(bundleFilePath.c_str(), ifstream::in);
    if(!bundleFile.is_open()) {
        printf("\nError reading bundle file");
        return -1;
    }

    string bundleOutFilePath = bundleOutPath + "/bundle.out";
    ofstream bundleOutFile(bundleOutFilePath.c_str(), ofstream::out);
    if(!bundleOutFile) {
        printf("\nCould not open output file");
        return -1;
    }
    
    string locOutFilePath = bundleOutPath + "/localized_queries.txt";
    ofstream localizedOutFile(locOutFilePath.c_str(), ofstream::out);
    if(!localizedOutFile) {
        printf("\nCould not open output file");
        return -1;
    }

    string camFileName, trackFileName;
    
    string line;
    getline(bundleFile, line);
    bundleOutFile << line << endl;

    int numImages, numPoints;
    getline(bundleFile,line);
    stringstream ss(line);
    ss >> numImages >> numPoints;
    bundleOutFile << line << endl;

    printf("\nNum cams %d, num pts %d", numImages, numPoints);
    fflush(stdout);

    int numCamsAdded = 0;
    map<int, vector<pointstr> > pointSet;

    for(int i=0; i < numImages; i++) {
        printf("\nImage %d", i);
        fflush(stdout);
        getline(bundleFile, line);
        if(line == "0 0 0") {
            for(int l=0; l < 4; l++)
                getline(bundleFile, line);

            printf("\nCamera %d non localized, trying to find cam file", i);
            stringstream strm;
            strm << camFilesPath << setfill('0') << setw(4) 
                << i <<"-cam-bdl.txt";
            camFileName = strm.str();
            ifstream camFile(camFileName.c_str(), ifstream::in);
            if(!camFile.is_open()) {
                printf("\nNo cam file/could not open cam file %s", camFileName.c_str());
                for(int i=0; i < 5; i++) {
                    bundleOutFile << "0 0 0\n";
                }
            } else {
                printf("\nFound cam file %s, dumping parameters", camFileName.c_str());
                while(getline(camFile,line)) {
                    bundleOutFile << line << endl;
                }

                localizedOutFile << i << endl;
            }
            strm.str("");
            strm << camFilesPath << setfill('0') << setw(4) 
                << i <<"-trk-bdl.txt";
            trackFileName = strm.str();

            ifstream trackFile(trackFileName.c_str(), ifstream::in);
            if(!trackFile.is_open()) {
                printf("\nNo track file/could not open track file %s", trackFileName.c_str());
            } else {
                numCamsAdded++;
                printf("\nFound track file %s", trackFileName.c_str());
                string line;
                while(getline(trackFile,line)) {
                    stringstream strm(line);
                    int ptIdx;
                    pointstr pt;
                    strm >> ptIdx;
                    strm >> pt.viewId;
                    strm >> pt.siftId;
                    strm >> pt.xcord;
                    strm >> pt.ycord;

                    map<int, vector<pointstr> >::iterator it;
                    it = pointSet.find(ptIdx);
                    if(it == pointSet.end()) {
                        vector<pointstr> initVec;
                        initVec.push_back(pt);
                        pointSet.insert(make_pair(ptIdx, initVec));
                    } else {
                        vector<pointstr>& pointVec = (*it).second;
                        pointVec.push_back(pt);
                    }
                } 
            }
        } else {
            bundleOutFile << line << endl;
            for(int l=0; l < 4; l++) { 
                getline(bundleFile,line);
                bundleOutFile << line << endl;
            } 
        }
    }

    int numPtsChanged;
    for(int i=0; i < numPoints; i++) {
        getline(bundleFile, line);
        bundleOutFile << line << endl;

        getline(bundleFile, line);
        bundleOutFile << line << endl;

        getline(bundleFile, line);
        stringstream ss(line);
        int numEle;
        ss >> numEle;

        vector<pointstr> vec;
        for(int e=0; e < numEle; e++) {
            pointstr pt;
            ss >> pt.viewId;
            ss >> pt.siftId;
            ss >> pt.xcord;
            ss >> pt.ycord;
            vec.push_back(pt);
        }

        map<int, vector<pointstr> >::iterator it;
        it = pointSet.find(i);
        if(it != pointSet.end()) {
            numPtsChanged++;
            vector<pointstr> &vec1 = (*it).second;
            for(int p=0; p < vec1.size(); p++) {
                vec.push_back(vec1[p]);
            }
            
            stringstream strm;
            strm.str("");
            strm << vec.size();
            for(int p=0; p < vec.size(); p++) {
                strm << " " << vec[p].viewId;                
                strm << " " << vec[p].siftId;                
                strm << " " << vec[p].xcord;                
                strm << " " << vec[p].ycord;                
            }
            bundleOutFile << strm.str() << endl;
        } else {
            bundleOutFile << line << endl;
        }
    }
   
    bundleOutFile.close();
    localizedOutFile.close();
    printf("\nCameras added %d, points appended %d", numCamsAdded, numPtsChanged);
}

