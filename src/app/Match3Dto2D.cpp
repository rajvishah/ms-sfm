#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../base/Reader.h"
#include "../base/Bundle.h"
#include "../base/keys2a.h"

#include "vector.h"

#include "../base/Localize.h"
using namespace std;
int main( int argc, char* argv[]) {

    if(argc < 4) {
        printf("Usage : ./localize <baseDir> <level> <imageIdx>\n");
        return -1;
    }


    string imageIdxStr = argv[3];
    int imageIdx = atoi(imageIdxStr.c_str());
    
    string levelStr = argv[2];
    int level = atoi(levelStr.c_str());
    
    string listFileName = string(argv[1]) + "/coarse_model/list.txt";

    stringstream stream;
    stream << argv[1] << "/initial_matching/ranks/file-" 
       << imageIdx << ".txt.srt";
    string rankFileName = stream.str();

    string bundlePath;
    if(level==-1) {
        bundlePath = string(argv[1]) + "/coarse_model/bundle/";
    } else {
        stringstream strm;
        strm << argv[1] << "/dens" << setw(1) << level << "/";
        bundlePath = strm.str();
    }

    ifstream listFile(listFileName.c_str(), std::ifstream::in);
    if(!listFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }

    string imageString;
    vector<string> keyFileNames;
    string line;
    int line_count = 0;
    while(getline(listFile, line)) {
        if(line_count == imageIdx) {
            imageString = line;
        }
        stringstream strm(line);
        string str;
        std::getline(strm, str, '.');
        string keyFileName = str + ".key";
        keyFileNames.push_back(keyFileName);
        line_count++;
    }
    listFile.close();
 
    bundle::Bundle bdl;
    reader::BundleReader br(bundlePath, &bdl); 
    br.read(&bdl);

    ifstream rankFile(rankFileName.c_str());
    if(!rankFile.is_open()) {
        cout << "\nError opening rank File";
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
    keypt_t* refKeysInfo;
    int numRefKeys = ReadKeyFile( keyFileNames[imageIdx].c_str(), 
            &refKeys, &refKeysInfo );

    ANNkd_tree *tree = CreateSearchTree( numRefKeys, refKeys);
    annMaxPtsVisit(300);

    vector< unsigned char* > queryKeys( nearbyImages.size() );
    vector< int > numFeatures( nearbyImages.size() );
    for(int i=0; i < nearbyImages.size(); i++) {
        int idx = nearbyImages[i];
        numFeatures[i] = ReadKeyFile( keyFileNames[idx].c_str() , &queryKeys[i] );
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
            unsigned char* qKey = queryKeys[i] + 128*keyIdx;
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

    vector< v3_t > pt3;
    vector< v2_t > pt2;
    vector< int > pt3_idx;
    vector< int > pt2_idx;
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
            pt3_idx.push_back(matches3D_2D[count].first);
            pt2_idx.push_back(matches3D_2D[count].second);
            
            const bundle::Vertex* vert = bdl.getVertex(matches3D_2D[count].first);
            v3_t p3 = v3_new(vert->mPos[0], vert->mPos[1], vert->mPos[2]);

            float x = refKeysInfo[matches3D_2D[count].second].x;
            float y = refKeysInfo[matches3D_2D[count].second].y;
            v2_t p2 = v2_new(x, y);

            pt3.push_back(p3);
            pt2.push_back(p2);
        }
        count = count1;
    }


    printf("\nFound %d good 3D-2D correspondences\n", finalMatches.size());

    //Prepare Structures to Register Camera
    
    if(finalMatches.size() == 0) {
        printf("\nNo matches to initiate registration, quitting");
        return 0;
    }

    localize::ImageData data;
    char* imageStr = strdup(imageString.c_str());
    data.InitFromString(imageStr, ".", false);
    data.m_licensed = true;
    NormalizeKeyPoints(numRefKeys, refKeysInfo, 
            data.GetWidth(), data.GetHeight());

    for(int i=0; i < numRefKeys; i++) {
        localize::KeypointWithDesc kpd(refKeysInfo[i].nx, refKeysInfo[i].ny, 
                refKeys + 128*i);
        data.m_keys_desc.push_back(kpd);
    } 


    bool status = BundleRegisterImage(data, pt3, pt2, pt3_idx, pt2_idx);
    if(status) {
        data.WriteCamera();
        data.WriteTracks();
    }


    return -1;
}
