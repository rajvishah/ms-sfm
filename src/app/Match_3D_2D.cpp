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

    if(argc < 5) {
        printf("Usage : ./matcher_3d_2d <bundlePath> <listPath> <queryImagePath> <rankFilePath> <outPath> <queryIdx>\n");
        return -1;
    }

    string bundlePath = string(argv[1]);
    string listsPath = string(argv[2]);
    string queryFileName = string(argv[3]);
    string rankFileName = string(argv[4]);
    string corrFilePath = string(argv[5]);
    string queryIdxStr = argv[6];
    string nearbyNumStr = argv[7];

    int numMaxNearby = atoi(nearbyNumStr.c_str());
    int queryIdx = atoi(queryIdxStr.c_str());

    clock_t start1 = clock();    
    reader::ImageListReader imList( listsPath );
    reader::KeyListReader keyList( listsPath );
    bool s1 = imList.read();
    bool s2 = keyList.read();
    if(!s1 || !s2) {
        printf("\nCouldn not read key/image/dim list");
        return -1;
    }


    ifstream queryFile( queryFileName.c_str(), ifstream::in);
    if( ! queryFile.is_open() ) {
        printf("\nCould not open query file");
        return -1;
    }

    vector<int> queryToImage;
    map<int,string> queryMap;
    string line;
    while(getline(queryFile, line)) {
        stringstream ss(line);
        int qId;
        string qName;
        ss >> qId >> qName;
        queryMap.insert(make_pair(qId, qName)); 
        queryToImage.push_back(qId);
    }
   
    int imageIdx = queryToImage[queryIdx];
    
    /*
    ifstream listFile(listFileName.c_str(), std::ifstream::in);
    if(!listFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }
    string imageString;
    vector<string> keyFileNames;
    int line_count = 0;
    while(getline(listFile, line)) {
        string readString = line;
        stringstream strm(line);
        string str;
        std::getline(strm, str, '.');
        string keyFileName = str + ".key";
        keyFileNames.push_back(keyFileName);
        if(line_count == imageIdx) {
            string imageString = str + ".jpg";
        }
        line_count++;
    }
    listFile.close();
    */

    bundle::Bundle bdl;
    reader::BundleReader br(bundlePath, &bdl); 
    bool status = br.read(&bdl);
    if(!status) {
        printf("\nError reading bundle file");
    }

    // Find all nearby images based on ranks
    ifstream rankFile(rankFileName.c_str());
    if(!rankFile.is_open()) {
        cout << "\nError opening rank File";
        return -1;
    }

    vector< pair<int, int> > nearbyWithScores;
    vector< pair<int, int> > allNearbyImages;
    while(getline(rankFile, line)) {
        istringstream str(line);
        int a, b,c;
        str >> a >> b >> c;
        if( a < imageIdx) continue;
        if( a > imageIdx) break;
        if(bdl.validTriangulated[b]) {
            allNearbyImages.push_back(make_pair(c,b));
        }
    }
    clock_t end1 = clock();    
    printf("[Match_3D_2D] Reading various list files took %0.6fs\n", 
            (end1 - start1) / ((double) CLOCKS_PER_SEC));

    clock_t start2 = clock();
    sort(allNearbyImages.begin(), allNearbyImages.end(), greater<pair <int,int> >());
    clock_t end2 = clock();    
    printf("[Match_3D_2D] Sorting ranks files took %0.6fs\n", 
            (end2 - start2) / ((double) CLOCKS_PER_SEC));

    int maxNearby = numMaxNearby < allNearbyImages.size() ? numMaxNearby : allNearbyImages.size(); 
    printf("\nMax nearby is %d", maxNearby);
    for(int i=0; i < maxNearby; i++) {
        nearbyWithScores.push_back(allNearbyImages[i]);
    }

    rankFile.close();
    unsigned char* refKeys;
    keypt_t* refKeysInfo;
    
    clock_t start3 = clock();
    // Read keys of Image to localize 
    // Create a tree reference to which 3D points will be matched
    int numRefKeys = ReadKeyFile( keyList.getKeyName(imageIdx).c_str(), 
            &refKeys, &refKeysInfo );

    ANNkd_tree *tree = CreateSearchTree( numRefKeys, refKeys);
    annMaxPtsVisit(300);
    clock_t end3 = clock();    
    printf("[Match_3D_2D] Creating query search tree took %0.6fs\n", 
            (end3 - start3) / ((double) CLOCKS_PER_SEC));

    clock_t start4 = clock();
    // Read all nearby image features
    vector< unsigned char* > queryKeys( nearbyWithScores.size() );
    vector< int > numFeatures( nearbyWithScores.size() );
 
    // Create a query list of all 3D points visible in nearby images
    map <int, vector< pair< int, int> > > pt2TrackMap;
    map <int, vector< pair< int, int> > >::iterator itr;
    vector< pair<int, int> > matches3D_2D;
    for(int i=0; i < nearbyWithScores.size(); i++) {
        printf("\nImage %d", i);
        int idx = nearbyWithScores[i].second;
        numFeatures[i] = ReadKeyFile( keyList.getKeyName(idx).c_str() , &queryKeys[i] );
        printf("\nReading file %d with score %d", idx, nearbyWithScores[i].first);
        
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

    printf("\nFound total %d 3D points", pt2TrackMap.size());
    clock_t end4 = clock();    
    printf("[Match_3D_2D] Reading nearby keys and forming tracks took %0.6fs\n", 
            (end4 - start4) / ((double) CLOCKS_PER_SEC));

    clock_t start5 = clock();
    // For each 3D point, match the corresponding 2D features with ref tree
    for(itr = pt2TrackMap.begin(); itr != pt2TrackMap.end(); itr++) {
        int pt3Idx = (*itr).first;
        vector< pair< int, int > >& track = (*itr).second;
        if(track.size() < 2) {
            continue;
        }

        map< int, int > matches;
        float minDist = 1000000;
        int minDistIdx = -1;
        bool foundMatch = false;
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

            foundMatch = true;
            int matchingPt = (int)indices[0];
            indices.clear();
            dists.clear();

            map< int, int >::iterator itr;
            itr = matches.find(matchingPt);
            if(itr != matches.end()) {
                (*itr).second++;
            } else {
                matches.insert( make_pair(matchingPt, 1));
                if( sqrt(bestDist) < minDist ) {
                    minDist = sqrt(bestDist);
                    minDistIdx = matchingPt;
                }
            }    
        }

        map< int, int >::iterator itr;
        int maxCount = 0;
        int maxIdx = -1;

        int counter = 0;
        for(itr = matches.begin(); itr != matches.end(); itr++) {
            if((*itr).second > maxCount) {
                maxCount = (*itr).second;
                maxIdx = (*itr).first;
            }
        }
       
        if(foundMatch) {
        if(maxCount > 1) {
            printf("\nPt3 %d found Pt2 %d Match with %d consensus for %d candidates", pt3Idx, maxIdx, maxCount, matches.size());
            matches3D_2D.push_back(make_pair(pt3Idx, maxIdx));
        } else {
            printf("\nPt3 %d found Pt2 %d Match with min distance %f", pt3Idx, minDistIdx, minDist);
            matches3D_2D.push_back(make_pair(pt3Idx, minDistIdx)); 
        }
        }
    }
    clock_t end5 = clock();    
    printf("[Match_3D_2D] 3D_2D_search took %0.6fs\n", 
            (end5 - start5) / ((double) CLOCKS_PER_SEC));

    printf("\nFound %d matches", matches3D_2D.size()); 

    clock_t start6 = clock();    

    int imageWidth= imList.getImageWidth(imageIdx);
    int imageHeight= imList.getImageHeight(imageIdx);
    NormalizeKeyPoints(numRefKeys, refKeysInfo, imageWidth, imageHeight);
 
    vector< pair<int, int> >& finalMatches2 = matches3D_2D;
    if(finalMatches2.size() < 16) {
        printf("\nNot enough matches to initiate registration, quitting");
        return 0;
    }
    vector< v3_t > pt3;
    vector< v2_t > pt2;
    vector< int > pt3_idx;
    vector< int > pt2_idx;
   
    stringstream stream1;
    stream1 << corrFilePath 
        << "matches_3D-2D_img-" << setw(4) << setfill('0') 
        << imageIdx << ".txt"; 
    string corrFileName = stream1.str();
    ofstream corrFile(corrFileName.c_str(), std::ofstream::out);
    if(!corrFile.is_open()) {
        cout << "\nError opening File " << corrFileName;
        return -1;
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

        corrFile << finalMatches2[count].second << " ";
        corrFile << finalMatches2[count].first << " ";
        corrFile << Vx(p2) << " ";
        corrFile << Vy(p2) << endl;
        corrFile << Vx(p3) << " ";
        corrFile << Vy(p3) << " ";
        corrFile << Vz(p3) << " ";
    }
    clock_t end6 = clock();    
    printf("[Match_3D_2D] Writing 3D_2D corr took %0.6fs\n", 
            (end6 - start6) / ((double) CLOCKS_PER_SEC));

    printf("\nFound %d good 3D-2D correspondences\n", finalMatches2.size());
    
    printf("[Match_3D_2D]  Total search for 3D_2D corr took %0.6fs\n", 
            (end6 - start1) / ((double) CLOCKS_PER_SEC));

    return -1;
}
