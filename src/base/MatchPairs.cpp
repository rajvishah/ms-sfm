#include "defs.h"
#include "MatchPairs.h"
#include "Geometric.h"
#include "Gridder.h"
#include "Matcher.h"
#include "matrix.h"

#include <set>
#include <algorithm>
#include <time.h>
using namespace sfm;

MatchPairs::MatchPairs(string& baseDir, int numNearby) :
    br(baseDir), imgList(baseDir), keyList(baseDir) {
        initialized = false;
        bool s1 = imgList.read();
        bool s2 = keyList.read();
        bool s3 = br.read(&bdl);
        numMaxCandidates = numNearby;
        numMinCovisible = 16;


        int numImages = bdl.getNumImages();
        query2locIdx.resize(numImages);
        if(s1 && s2 && s3) {
            for(int i = 0; i < numImages; i++) {
                if(bdl.validTriangulated[i]) {
                    localizedImages.push_back(i);
                    query2locIdx[i] = localizedImages.size() - 1;
                } 
            }
            initialized = true;
        }
    }

int MatchPairs::findUniquePairs() {
    if(candidateLists.size() == 0) {
        findAllCandidateImages();
    }

    map< pair< int,int > , int> uniquePairs;
    for(int i=0; i < localizedImages.size(); i++) {
        int idx1 = localizedImages[i];
        //        printf("\n");
        for(int j=0; j < candidateLists[i].size(); j++) {
            int idx2 = candidateLists[i][j];
            int firstId = idx1 < idx2 ? idx1 : idx2;
            int secondId = idx1 < idx2 ? idx2 : idx1;

            pair< int,int > p(firstId, secondId);
            uniquePairs.insert(make_pair(p,1));
        } 
    }

    map< pair< int,int > , int>::iterator itr;
    itr = uniquePairs.begin();
    while(itr != uniquePairs.end()) {
        pair<int, int> p = (*itr).first;
        candidatePairs.push_back(p);
        itr++;

        //Find Fundamental Matrix
        int firstId = p.first;
        int secondId = p.second;
        double* P1 = bdl.getP_orig( firstId );
        double* P2 = bdl.getP_orig( secondId );
        const bundle::CamStruct* cs = bdl.getCamSet( firstId );
        double C[4] = {cs->t[0], cs->t[1], cs-> t[2], 1};

        vector<double> F(9);
        vector<double> FT(9);

        geometry::ComputeFundamental(P1, P2, C, F.data());
        matrix_transpose(3,3,F.data(),FT.data());

        candidatePairsF.push_back(F);
        candidatePairsFT.push_back(FT);


        if(firstId == 0 && secondId == 54) {
          printf("\nP1:");
          for(int ii=0; ii < 12; ii++) {
            printf("%lf ", P1[ii]);
          }
          printf("\nP2:");
          for(int ii=0; ii < 12; ii++) {
            printf("%lf ", P2[ii]);
          }
          printf("\nF:");
          for(int ii=0; ii < 9; ii++) {
            printf("%lf ", F[ii]);
          }
        
        }
    }
    return (int)(candidatePairs.size());
}

bool MatchPairs::writeUniquePairs(string& resultDir, int numLists) { 
    if(candidatePairs.empty()) {
        cout << "\nNo candidate pairs to write";
        return false;
    }

    int numPairs = candidatePairs.size();
    int numPairsPerList = ceil(numPairs*1.0f/numLists);

    int count = 0;
    FILE* fp = NULL;
    char filename[1000];
    for(int i=0; i < numPairs; i++) {
        if(i%numPairsPerList == 0) {
            count++;
            if(fp != NULL) {
                printf("\nWriting pair file %s", filename);            
                fclose(fp);
            }
            sprintf(filename, "%s/pairs-%04d.txt", resultDir.c_str(), count);
            fp = fopen(filename, "w");
            if(fp == NULL) {
                printf("\nError opening pair file to write");
                return false;
            }
        }
        pair<int, int> p = candidatePairs[i];
        fprintf(fp, "%d %d", p.first, p.second); 
        vector<double> F = candidatePairsF[i];
        for(int f=0; f < 9; f++) {
            fprintf(fp," %lf", F[f]);
        }
        fprintf(fp,"\n");
    }

    return true;
}


bool MatchPairs::matchListPairs(string& listPath ,string& resultDir) {
    int counter = 0;

    ifstream pairListFile( listPath.c_str(), std::ifstream::in );
    if(!pairListFile.is_open()) {
        cout << "\nError opening list file " << listPath;
        return false;
    }

    vector< pair<int, int> > candPairs;
    set<int> images;
    vector< vector<double> > candPairsF;
    vector< vector<double> > candPairsFT;
    string line;
    while(getline(pairListFile, line)) {
        int first, second;
        istringstream str(line);
        str >> first >> second;
        vector<double> F(9);
        vector<double> FT(9);
        for(int i=0; i < 9; i++) {
            str >> F[i];
        }
        matrix_transpose(3,3,F.data(),FT.data());
        images.insert(first);
        images.insert(second);
        candPairs.push_back(make_pair(first,second));
        candPairsF.push_back(F);
    }

    sort(candPairs.begin(), candPairs.end());

    int numKeys = keyList.getNumKeys();

    vector< unsigned char* > keys(numKeys);
    vector< keypt_t* > keysInfo(numKeys);
    vector< int > numFeatures(numKeys);
    vector< Gridder > grids(numKeys);
    bool normalized = true;

    set<int>::iterator it;
    for(it=images.begin(); it != images.end(); it++) {
        int idx = (*it);
        string keyPath = keyList.getKeyName(idx);
        numFeatures[idx] = ReadKeyFile(keyPath.c_str(),
                &keys[idx], &keysInfo[idx]);
        int W = imgList.getImageWidth(idx);
        int H = imgList.getImageHeight(idx);

        NormalizeKeyPoints(numFeatures[idx], keysInfo[idx], W, H);

        Gridder currGrid(16, W, H, numFeatures[idx], keysInfo[idx]);
        grids[idx] = currGrid;
    }

    clock_t start = clock();

    while(counter < candPairs.size()) {
        pair<int, int> p = candPairs[counter]; 

        int queryIdx = p.first;

        unsigned char* queryKey = keys[queryIdx]; 
        keypt_t* queryKeyInfo = keysInfo[queryIdx];

        int qW = imgList.getImageWidth(queryIdx);
        int qH = imgList.getImageHeight(queryIdx);

        vector< vector<double> > srcRectEdges; 
        geometry::ComputeRectangleEdges((double)qW,
                (double)qH, srcRectEdges, normalized); 

        const string& queryImgName = imgList.getImageName(queryIdx); 

        do {
            pair<int, int> p = candPairs[counter]; 
            int refIdx = p.second;

            char file1[1024], file2[1024];
            sprintf(file1, "%s/%d-%d.txt", resultDir.c_str(), queryIdx, refIdx);
            sprintf(file2, "%s/%d-%d.txt", resultDir.c_str(), refIdx, queryIdx);

            FILE* currFile = fopen(file1, "w");
            if(currFile == NULL) {
                printf("\nError opening file %s", file1);
            }


            unsigned char* refKey = keys[refIdx];
            keypt_t* refKeyInfo = keysInfo[refIdx];

            int rW = imgList.getImageWidth(refIdx);
            int rH = imgList.getImageHeight(refIdx);

            vector< vector<double> > refRectEdges; 

            geometry::ComputeRectangleEdges((double)rW,
                    (double)rH, refRectEdges, normalized);
 
            fflush(stdout);

            vector<double>& fMatrix = candPairsF[counter];
            const string& refImgName = imgList.getImageName(refIdx); 

            match::FeatureMatcher matcher;

            //matcher.queryImage = cv::imread(queryImgName.c_str(), CV_LOAD_IMAGE_COLOR);

            //matcher.referenceImage = cv::imread(refImgName.c_str(), CV_LOAD_IMAGE_COLOR);
            matcher.setNumSrcPoints( numFeatures[queryIdx] );
            matcher.setSrcKeys(queryKeyInfo, queryKey);
            matcher.setNumRefPoints( numFeatures[refIdx] );
            matcher.setRefKeys(refKeyInfo, refKey);

            matcher.setImageDims(qW, qH, rW, rH);

            matcher.setSrcRectEdges(srcRectEdges);
            matcher.setRefRectEdges(refRectEdges);

            matcher.setQueryGrid(&grids[queryIdx]);
            matcher.setRefGrid(&grids[refIdx]);

            matcher.setFMatrix(fMatrix);
            matcher.computeEpipolarLines();
            matcher.clusterPointsFast();

            int numMatches = matcher.match();            
            counter = counter + 1;


            printf("\n%d and %d : matches found %d", queryIdx, refIdx, numMatches);

            if(numMatches >= 16) {
            //matcher.visualizeMatches(NULL);
                printf("Writing %d matches between images %d and %d\n",
                        numMatches, queryIdx, refIdx);
                sort(matcher.matches.begin(), matcher.matches.end());

                for(int m=0; m < matcher.matches.size(); m++) {
                    int viewId1 = queryIdx;
                    int siftId1 = matcher.matches[m].first;
                    float x1 = queryKeyInfo[siftId1].nx;
                    float y1 = queryKeyInfo[siftId1].ny;
                    int viewId2 = refIdx;
                    int siftId2 = matcher.matches[m].second;
                    float x2 = refKeyInfo[siftId2].nx;
                    float y2 = refKeyInfo[siftId2].ny;


                    fprintf(currFile, "%d %d %lf %lf %d %d %lf %lf\n", viewId1, siftId1, x1, y1, viewId2, siftId2, x2, y2);
                }
            }


            fclose(currFile);
        } while(candPairs[counter].first == queryIdx);

    }

    clock_t end = clock();    
    printf("[KeyMatchGeoAware] Matching took %0.3fs for %s\n", 
            (end - start) / ((double) CLOCKS_PER_SEC), listPath.c_str());
    fflush(stdout);
    return true;
}


bool MatchPairs::matchAllPairs(string& resultDir) {
    int counter = 0;

    string matchFileName = resultDir + "/matches.txt";
    ofstream matchFile( matchFileName.c_str(), std::ofstream::out );
    if(!matchFile.is_open()) {
        cout << "\nError opening match file to write";
        return false;
    }

    int numKeys = keyList.getNumKeys();

    vector< unsigned char* > keys(numKeys);
    vector< keypt_t* > keysInfo(numKeys);
    vector< int > numFeatures(numKeys);
    vector< Gridder > grids;
    bool normalized = true;

    vector< FILE* > files(numKeys);

    for(int i=0; i < keyList.getNumKeys(); i++) {
        string keyPath = keyList.getKeyName(i);
        numFeatures[i] = ReadKeyFile(keyPath.c_str(),
                &keys[i], &keysInfo[i]);
        int W = imgList.getImageWidth(i);
        int H = imgList.getImageHeight(i);

        NormalizeKeyPoints(numFeatures[i], keysInfo[i], W, H);

        Gridder currGrid(16, W, H, numFeatures[i], keysInfo[i]);
        grids.push_back(currGrid);

    }

    clock_t start = clock();

    while(counter < candidatePairs.size()) {
        pair<int, int> p = candidatePairs[counter]; 

        int queryIdx = p.first;

        unsigned char* queryKey = keys[queryIdx]; 
        keypt_t* queryKeyInfo = keysInfo[queryIdx];

        int qW = imgList.getImageWidth(queryIdx);
        int qH = imgList.getImageHeight(queryIdx);

        vector< vector<double> > srcRectEdges; 
        geometry::ComputeRectangleEdges((double)qW,
                (double)qH, srcRectEdges, normalized); 

        const string& queryImgName = imgList.getImageName(queryIdx); 

        do {
            pair<int, int> p = candidatePairs[counter]; 
            int refIdx = p.second;

            char file1[1024];
            sprintf(file1, "%s/%d-%d.txt", resultDir.c_str(), queryIdx, refIdx);

            FILE* currFile = fopen(file1, "w");
            if(currFile == NULL) {
                printf("\nError opening file %s", file1);
            }


            unsigned char* refKey = keys[refIdx];
            keypt_t* refKeyInfo = keysInfo[refIdx];

            int rW = imgList.getImageWidth(refIdx);
            int rH = imgList.getImageHeight(refIdx);

            vector< vector<double> > refRectEdges; 

            geometry::ComputeRectangleEdges((double)rW,
                    (double)rH, refRectEdges, normalized);
 
            fflush(stdout);


            vector<double>& fMatrix = candidatePairsF[counter];
            const string& refImgName = imgList.getImageName(refIdx); 

            match::FeatureMatcher matcher;

            //matcher.queryImage = cv::imread(queryImgName.c_str(), CV_LOAD_IMAGE_COLOR);

            //matcher.referenceImage = cv::imread(refImgName.c_str(), CV_LOAD_IMAGE_COLOR);
            matcher.setNumSrcPoints( numFeatures[queryIdx] );
            matcher.setSrcKeys(queryKeyInfo, queryKey);
            matcher.setNumRefPoints( numFeatures[refIdx] );
            matcher.setRefKeys(refKeyInfo, refKey);

            matcher.setImageDims(qW, qH, rW, rH);

            matcher.setSrcRectEdges(srcRectEdges);
            matcher.setRefRectEdges(refRectEdges);

            matcher.setQueryGrid(&grids[queryIdx]);
            matcher.setRefGrid(&grids[refIdx]);

            matcher.setFMatrix(fMatrix);
            matcher.computeEpipolarLines();
            matcher.clusterPointsFast();

            int numMatches = matcher.match();            
            counter = counter + 1;


            printf("\n%d and %d : matches found %d", queryIdx, refIdx, numMatches);

            if(numMatches >= 16) {
            //matcher.visualizeMatches(NULL);
                printf(" Writing %d matches between images %d and %d",
                        numMatches, queryIdx, refIdx);
                sort(matcher.matches.begin(), matcher.matches.end());
                matchFile << queryIdx << " " << refIdx << endl;
                matchFile << numMatches << endl;

                for(int m=0; m < matcher.matches.size(); m++) {
                    matchFile << matcher.matches[m].first 
                        << " " << matcher.matches[m].second << endl;

                    int viewId1 = queryIdx;
                    int siftId1 = matcher.matches[m].first;
                    float x1 = queryKeyInfo[siftId1].nx;
                    float y1 = queryKeyInfo[siftId1].ny;
                    int viewId2 = refIdx;
                    int siftId2 = matcher.matches[m].second;
                    float x2 = refKeyInfo[siftId2].nx;
                    float y2 = refKeyInfo[siftId2].ny;


                    fprintf(currFile, "%d %d %lf %lf %d %d %lf %lf\n", viewId1, siftId1, x1, y1, viewId2, siftId2, x2, y2);
                }
            }


            fclose(currFile);
        } while(candidatePairs[counter].first == queryIdx);

    }

    clock_t end = clock();    
    printf("[KeyMatchGeoAware] Matching took %0.3fs\n", 
            (end - start) / ((double) CLOCKS_PER_SEC));
    fflush(stdout);
    matchFile.close();
}

void MatchPairs::findAllCandidateImages(bool tryHard) {
    totalCandidatePairs = 0;
    candidateLists.resize(localizedImages.size());
    for(int i=0; i < localizedImages.size(); i++) {
        bool status = findCandidateImages(localizedImages[i], tryHard);
        totalCandidatePairs += candidateLists[i].size();
    }
}

bool MatchPairs::findSelectedCandidateImages(string& idFile, bool tryHard) {
    totalCandidatePairs = 0;
    candidateLists.resize(localizedImages.size());

    ifstream queryListFile( idFile.c_str(), std::ifstream::in );
    if(!queryListFile.is_open()) {
        cout << "\nError opening list file " << idFile;
        return false;
    }

    vector<int> queryImages;
    string line;
    while(getline(queryListFile,line)) {
        int imageIdx;
        istringstream lineStream(line);
        lineStream >> imageIdx;
        queryImages.push_back(imageIdx);
    }

    for(int i=0; i < queryImages.size(); i++) {
        bool status = findCandidateImages(queryImages[i], tryHard);
        int locIdx = query2locIdx[queryImages[i]];
        totalCandidatePairs += candidateLists[locIdx].size();
    }
    return true;
}


bool MatchPairs::findCandidateImages(int qImgId, bool tryHard) {
    const vector< pair <int,int > >& qViewArr = bdl.viewArrRow(qImgId);

    map< int, vector<pair<int, int> > > viewPointMap;
    vector< pair<int, int> > ptList(qViewArr.size());
    for(int i=0; i < qViewArr.size(); i++) {
        int pt  = qViewArr[i].second;
        int key  = qViewArr[i].first;
        ptList[i] = make_pair(pt, key);
    }

    createPointHistogram(ptList, viewPointMap);
    vector<int> sortedList;

    sortAndTruncateHistogram(viewPointMap, 
            sortedList, numMinCovisible, qImgId); 

    if(sortedList.size() == 0) return false;
    if(tryHard && sortedList.size() < numMaxCandidates) {
        bool status = twoLevelNearbySearch(qImgId, viewPointMap,
                sortedList);
    }

    int locIdx = query2locIdx[qImgId];
    vector<int>& nearbyImgs = candidateLists[locIdx];

    if(sortedList.size() >= numMaxCandidates) {
        nearbyImgs.insert(nearbyImgs.end(), 
                sortedList.begin(), sortedList.begin() 
                + numMaxCandidates);
        return true;
    } 

    if(sortedList.size() >= 20) {
        nearbyImgs.insert(nearbyImgs.end(), 
                sortedList.begin(), sortedList.end()); 
        return true;
    }

    return false;
}

bool MatchPairs::twoLevelNearbySearch(int qImgId,
        map<int, vector<pair<int, int> > >& viewToPt,
        vector< int >& sortedList) {

    int numNearToFill = numMaxCandidates 
        - sortedList.size();

    vector< map<int, vector<pair<int, int> > > > 
        hist(sortedList.size());

    map<int, vector<pair<int, int> > >::iterator itr;

    vector< vector<pair<int,int> > > ptLists;
    ptLists.resize(sortedList.size());

    vector< vector<int> > candViewLists(sortedList.size());

    int totalCovisiblePoints = 0;
    vector<int> numCovisiblePoints(sortedList.size());

    for(int i=0; i < sortedList.size(); i++) {
        int imgIdx = sortedList[i];
        itr = viewToPt.find(imgIdx);
        if(itr == viewToPt.end()) {
            printf("\nGrave concern!! This shouldn't happen");
        }
        numCovisiblePoints[i] = (int)(((*itr).second).size());
        totalCovisiblePoints += numCovisiblePoints[i];
        getCandidatePoints((*itr), ptLists[i]);
        createPointHistogram(ptLists[i], hist[i]);
        sortAndTruncateHistogram(hist[i], candViewLists[i], 
                numMinCovisible, imgIdx);

    }

    vector<int> dummyNearBy;
    dummyNearBy.insert(dummyNearBy.end(), sortedList.begin(), sortedList.end());
    vector<int> counter(sortedList.size(), 0);
    while(numNearToFill > 0) {
        int countLimReached = 0;
        for(int i=0; i < sortedList.size() ; i++) {
            if(counter[i] >= candViewLists[i].size()) {
                countLimReached++;
            }  
        }

        if(countLimReached == sortedList.size()) {
            break; 
        }

        for(int i=0; i < sortedList.size(); i++) {
            float weight = (float)numCovisiblePoints[i]
                /(float)totalCovisiblePoints;
            int numPointsToSelect = (int)ceil(weight*numNearToFill);
            int ctr = 0;
            while(ctr < numPointsToSelect 
                    && counter[i] < candViewLists[i].size()) {
                int view = candViewLists[i][counter[i]];
                vector<int>::iterator vItr;
                vItr = find(dummyNearBy.begin(), 
                        dummyNearBy.end(), view);
                counter[i]++;
                if(vItr == dummyNearBy.end() && view != qImgId) {
                    dummyNearBy.push_back(view);
                    ctr++;
                    numNearToFill = numNearToFill - 1; 
                }
            } 
        }
    }

    sortedList.insert(sortedList.end(), 
            dummyNearBy.begin(), dummyNearBy.end());
    return true;
}

void MatchPairs::createPointHistogram(
        vector< pair<int, int> >& pointList,  
        map<int, vector<pair<int, int> > > &hist) {
    for(int i=0; i < pointList.size(); i++) {
        pair<int, int> p = pointList[i];
        int pt = p.first;
        const vector<	int > &ptToIm = bdl.pointToImage(pt);
        for(int j=0; j < ptToIm.size(); j++) {
            int img_j = ptToIm[j];

            if(hist.find(img_j) != hist.end()) {
                hist[img_j].push_back(p);
            } else {
                vector<pair<int, int> > initVec;
                initVec.push_back(p);
                hist.insert(make_pair(img_j, initVec));
            }
        }
    }
}

int MatchPairs::sortAndTruncateHistogram(
        map<int, vector<pair<int, int> > >&hist, 
        vector<int>& sortedList,
        int maxHeight, int queryId) {
    multimap< float,int,greater<float> > tempNearBy;
    map<int, vector<pair<int, int> > >::iterator 
        mapItr = hist.begin();
    while(mapItr != hist.end()) {
        int image_id = (*mapItr).first;
        if(image_id == queryId) {
            hist.erase(mapItr++);
            continue;
        }
        vector<pair< int, int> >& values = (*mapItr).second;
        pair<float,int> ele((float)values.size(), image_id);
        tempNearBy.insert(ele);
        mapItr++;
    }

    if(hist.size() != tempNearBy.size()) {
        printf("\nSome Problem");
        return 0;
    }

    multimap< float,int>::iterator mIt = tempNearBy.begin();
    while(mIt != tempNearBy.end()) {
        if((*mIt).first >= 16) {
            sortedList.push_back((*mIt).second);
        } else { 
            int view_id = (*mIt).second;
            mapItr = hist.find(view_id);
            if(mapItr != hist.end()) {
                hist.erase(mapItr); // erase 
            } else {
                printf("\n****ERROR****\n");
            }
        }
        mIt++;
    }

    return sortedList.size();
}

bool MatchPairs::getCandidatePoints(
        pair<const int, vector< pair<int, int> > >& ptList,
        vector< pair<int, int> >& candidates) {
    int viewIdx = ptList.first;
    const vector< pair <int, int> >& pointList 
        = bdl.viewArrRow(viewIdx);
    candidates.clear();
    for(int i=0; i < pointList.size(); i++) {
        int pt = pointList[i].second;
        int key = pointList[i].first;
        candidates.push_back(make_pair(pt, key));
    }
    return true;
}

