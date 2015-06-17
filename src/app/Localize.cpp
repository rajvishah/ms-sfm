#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../base/Reader.h"
#include "../base/Bundle.h"
#include "../base/keys2a.h"

using namespace std;
int main( int argc, char* argv[]) {

    if(argc < 4) {
        printf("Usage : ./localize <listFile> <rankFile> <bundlePath> <imageIdx>\n");
        return -1;
    }

    string listFileName = argv[1];
    string rankFileName = argv[2];
    string bundlePath = argv[3];
    string imageIdxStr = argv[4];
 
    int imageIdx = atoi(imageIdxStr.c_str());

    ifstream listFile(listFileName.c_str());
    if(!listFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }

    vector<string> keyFileNames;
    string line;
    while(getline(listFile, line)) {
        keyFileNames.push_back(line);
    }
    listFile.close();

    
    bundle::Bundle bdl;
    reader::ImageListReader imList( bundlePath );
    reader::BundleReader br(bundlePath, &bdl); 
    br.read(&bdl, imList);

    ifstream rankFile(rankFileName.c_str());
    if(!rankFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }

    
    vector<int> nearbyImages;
    while(getline(rankFile, line)) {
        istringstream str(line);
        int a, b,c;
        str >> a >> b >> c;
        if(bdl.validTriangulated[b]) {
            nearbyImages.push_back(b);
            if(nearbyImages.size() == 20) {
                break;
            }
        }
    }
    rankFile.close();

    unsigned char* refKeys;
    int numRefKeys = ReadKeyFile( keyFileNames[imageIdx].c_str(), &refKeys);
    ANNkd_tree *tree = CreateSearchTree( numRefKeys, refKeys);
    annMaxPtsVisit(300);

    vector< unsigned char* > queryKeys( nearbyImages.size() );
    vector< int > numFeatures( nearbyImages.size() );
    for(int i=0; i < nearbyImages.size(); i++) {
        int idx = nearbyImages[i];
        numFeatures[i] = ReadKeyFile( keyFileNames[idx].c_str() , &queryKeys[idx] );
    }

    
    vector< pair<int, int> > matches3D_2D;
    for(int i=0; i < nearbyImages.size(); i++) {
        printf("\nImage %d", i);
        int idx = nearbyImages[i];
        const vector< pair< int, int > >& keyPtPairs = bdl.viewArrRow(idx);

        for(int j=0; j < keyPtPairs.size(); j++) {
            vector<ANNidx> indices(2);
            vector<ANNdist> dists(2);
       
            int keyIdx = keyPtPairs[j].first;
            int pt3Idx = keyPtPairs[j].second;
            unsigned char* qKey = queryKeys[idx] + 128*j;
            tree->annkPriSearch(qKey, 2, indices.data(), dists.data(), 0.0);

            float bestDist = (float)(dists[0]);
            float secondBestDist = (float)(dists[1]);

            float distRatio = sqrt(bestDist/secondBestDist);
            if(distRatio > 0.6) {
                continue;
            }

            int matchingPt = (int)indices[0];
            indices.clear();
            dists.clear();

            matches3D_2D.push_back( make_pair(pt3Idx, matchingPt) ); 
        } 
        printf(" found %d matches", matches3D_2D.size());
    }

    vector< pair<int, int> > finalMatches;
    sort( matches3D_2D.begin(), matches3D_2D.end() );

    int count = 0;
    while( count < matches3D_2D.size() ) {
        int count1 = count + 1;
        pair<int,int> P = matches3D_2D[count];
        bool goodPair = true;
        while(matches3D_2D[count1].first == P.first) {
            if(matches3D_2D[count1].second != P.second) {
                goodPair = false;
            }
            count1++; 
        }
        if(goodPair) {
            finalMatches.push_back(matches3D_2D[count]);
        }
        count = count1;
    }


    printf("\nFound %d good 3D-2D correspondences\n", finalMatches.size());
    return -1;
}
