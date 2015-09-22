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
        strm << argv[1] << "dens" << setw(1) << level << "/";
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

    // Find all nearby images based on ranks
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
            if(nearbyImages.size() == 50) {
                break;
            }
        }
    }
    rankFile.close();
    unsigned char* refKeys;
    keypt_t* refKeysInfo;
    
    // Read keys of Image to localize 
    // Create a tree reference to which 3D points will be matched
    int numRefKeys = ReadKeyFile( keyFileNames[imageIdx].c_str(), 
            &refKeys, &refKeysInfo );

    ANNkd_tree *tree = CreateSearchTree( numRefKeys, refKeys);
    annMaxPtsVisit(300);

    // Read all nearby image features
    vector< unsigned char* > queryKeys( nearbyImages.size() );
    vector< int > numFeatures( nearbyImages.size() );
    for(int i=0; i < nearbyImages.size(); i++) {
        int idx = nearbyImages[i];
        numFeatures[i] = ReadKeyFile( keyFileNames[idx].c_str() , &queryKeys[i] );
    }
 
    // Create a query list of all 3D points visible in nearby images
    map <int, vector< pair< int, int> > > pt2TrackMap;
    map <int, vector< pair< int, int> > >::iterator itr;
    vector< pair<int, int> > matches3D_2D;
    for(int i=0; i < nearbyImages.size(); i++) {
        printf("\nImage %d", i);
        int idx = nearbyImages[i];
        const vector< pair< int, int > >& keyPtPairs = bdl.viewArrRow(idx);
        for(int j=0; j < keyPtPairs.size(); j++) {
            int keyIdx = keyPtPairs[j].first;
            int pt3Idx = keyPtPairs[j].second;

            // IF 3D point is in query list, append to its track (view, sift)
            itr = pt2TrackMap.find(pt3Idx);
            if(itr != pt2TrackMap.end()) {
                vector<pair<int, int> >& track = (*itr).second;
                track.push_back(make_pair(i, keyIdx));
            } else {
                vector<pair< int, int > > initVec;
                pt2TrackMap.insert(make_pair(pt3Idx, initVec));
            }
        }
    }

    // For each 3D point, match the corresponding 2D features with ref tree
    for(itr = pt2TrackMap.begin(); itr != pt2TrackMap.end(); itr++) {
        int pt3Idx = (*itr).first;
        vector< pair< int, int > >& track = (*itr).second;
        if(track.size() < 2) {
            continue;
        }

        map< int, int > matches;
        vector<float> matchDistances;
        for(int j=0; j < track.size(); j++) {
            vector<ANNidx> indices(2);
            vector<ANNdist> dists(2);
       
            int imgIdx = track[j].first;
            int keyIdx = track[j].second;
            
            unsigned char* qKey = queryKeys[imgIdx] + 128*keyIdx;
            tree->annkPriSearch(qKey, 2, indices.data(), dists.data(), 0.0);

            float bestDist = (float)(dists[0]);
            float secondBestDist = (float)(dists[1]);

            float distRatio = sqrt(bestDist/secondBestDist);
            if(distRatio > 0.6) {
                continue;
            }

            matchDistances.push_back(bestDist);
            int matchingPt = (int)indices[0];
            indices.clear();
            dists.clear();

            map< int, int >::iterator itr;
            itr = matches.find(matchingPt);
            if(itr != matches.end()) {
                (*itr).second++;
            } else {
                matches.insert( make_pair(matchingPt, 1));
            }
        }

        map< int, int >::iterator itr;
        int maxCount = 0;
        int maxIdx = -1;

        int minDistIdx = -1;
        float minDist = 1000000;
        int counter = 0;
        for(itr = matches.begin(); itr != matches.end(); itr++) {
            if((*itr).second > maxCount) {
                maxCount = (*itr).second;
                maxIdx = (*itr).first;
            }

            if( matchDistances[counter] < minDist) {
                minDist = matchDistances[counter];
                minDistIdx = counter;
            }
        }
       
        if(maxCount > 1) {
            printf("\nPt3 %d found Pt2 %d Match with %d consensus for %d candidates", pt3Idx, maxIdx, maxCount, matches.size());
            matches3D_2D.push_back(make_pair(pt3Idx, maxIdx));
        } else {
            printf("\nPt3 %d found Pt2 %d Match with min distance", pt3Idx, maxIdx);
            matches3D_2D.push_back(make_pair(pt3Idx, minDistIdx)); 
        }
    }

    printf("\nFound %d matches", matches3D_2D.size()); 

    /*
    vector< pair<int, int> > finalMatches1, finalMatches2;

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
            finalMatches1.push_back(make_pair
                    (matches3D_2D[count].second,matches3D_2D[count].first));
        }
        count = count1;
    }

    count = 0;
    sort(finalMatches1.begin(), finalMatches1.end());
    while( count < finalMatches1.size() ) {
        int count1 = count + 1;
        pair<int,int> P = finalMatches1[count];
        bool goodPair = true;
        while(finalMatches1[count1].first == P.first) {
            if(finalMatches1[count1].second != P.second) {
                goodPair = false;
            }
            count1++; 
        }

        if(goodPair) {
            finalMatches2.push_back(make_pair(
                    finalMatches1[count].second, 
                    finalMatches1[count].first));
        }
        count = count1;
    }

    */

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
    
    vector< v3_t > pt3;
    vector< v2_t > pt2;
    vector< int > pt3_idx;
    vector< int > pt2_idx;
   

    stringstream stream;
    stream << argv[1] << "/loc" << setw(1) << level << "/"; 
    string corrFileName = stream.str();
    ifstream corrFile(corrFileName.c_str(), std::ofstream::out);
    if(!corrFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }
    
    vector< pair<int, int> >& finalMatches2 = matches_3D_2D;
    if(finalMatches2.size() < 16) {
        printf("\nNot enough matches to initiate registration, quitting");
        return 0;
    }
    
    //for(int count=0; count < finalMatches2.size(); count++) {
    for(int count=0; count < finalMatches2.size(); count++) {
        pt3_idx.push_back(count);
        pt2_idx.push_back(count);

        const bundle::Vertex* vert = bdl.getVertex(finalMatches2[count].first);
        v3_t p3 = v3_new(vert->mPos[0], vert->mPos[1], vert->mPos[2]);

        printf("\nIdx : %d", finalMatches2[count].second);

        float x = refKeysInfo[finalMatches2[count].second].nx;
        float y = refKeysInfo[finalMatches2[count].second].ny;

        printf(" Point %f %f", x, y);
        v2_t p2 = v2_new(x, y);

        pt3.push_back(p3);
        pt2.push_back(p2);

        corrFile << finalMatches2[count].first << " "
        corrFile << finalMatches2[count].second << " "
        corrFile << Vx(p3) << " "
        corrFile << Vy(p3) << " "
        corrFile << Vz(p3) << " "
        corrFile << Vx(p2) << " "
        corrFile << Vy(p2) << endl;
    }


    printf("\nFound %d good 3D-2D correspondences\n", finalMatches2.size());

    return -1;
}
