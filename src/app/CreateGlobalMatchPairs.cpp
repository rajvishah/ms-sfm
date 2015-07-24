#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <math.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include <algorithm>
using namespace std;
int main(int argc, char* argv[]) {
    string keyList = argv[1];
    string resultPath = argv[2];
    string numListsStr = argv[3];

    int numLists = atoi( numListsStr.c_str() );

    ifstream keyListFile( keyList.c_str() );
    if(!keyListFile.is_open()) {
        cout << "\n COuld not open key list file" ;
        return -1;
    }

    vector<int> numFeatures;
    string line;
    while(getline( keyListFile, line)) {
        
        ifstream keyFile(line.c_str());
        if(!keyFile.is_open()) {
            cout << "\nCould not open file";
            return -1;
        }
        getline(keyFile, line);
        istringstream str(line);
        int nFeat;
        str >> nFeat;
        numFeatures.push_back(nFeat);

        keyFile.close();
    }

    int numImages = numFeatures.size();
    int numPairs = numImages*(numImages-1)/2;
    int numPairsPerList = floor(numPairs/numLists);
    int currList = 0;

    vector< pair<int, int> > pairs;
    for(int i=0; i < numFeatures.size(); i++) {
        for(int j=0; j < i; j++) {
            if( numFeatures[i] < numFeatures[j] ) {
                pairs.push_back(make_pair(i, j));
            } else {
                pairs.push_back(make_pair(j, i));
            } 
        }
    }

    sort( pairs.begin(), pairs.end());

    cout << "\nWriting lists " << numLists;
    for(int i=0; i < numLists; i++) {
        char filename[1000];
        sprintf(filename,"%s/initial-pairs-%d.txt",resultPath.c_str(),i);
        
        ofstream pairsOut( filename, std::ofstream::out );
        if( !pairsOut.is_open() ) {
            cout << "\nError opening pairs output file!";
        }

        int start = i*numPairsPerList;
        int end = (i+1)*numPairsPerList;

        if(i == numLists - 1) {
            end = numPairs;
        }
//        cout << endl << i << " " << numLists << " " << start << " " << end << " " << numPairs << endl;
        for(int j = start; j < end; j++) {
            pairsOut << pairs[j].first << " " << pairs[j].second << "\n";
        }
        pairsOut.close();
    }
    return 0;
}
