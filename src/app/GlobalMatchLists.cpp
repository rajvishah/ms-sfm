#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <set>
#include <time.h>

#include "../base/keys2a.h"
#include "../base/argvparser.h"


using namespace std;
using namespace CommandLineProcessing;

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
    cmd.setIntroductoryDescription("Match-graph Construction");

    //define error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help",""); 
    cmd.defineOption("ranks_list", "Output file to write image pair and num matches", ArgvParser::OptionRequired);

    cmd.defineOption("pairs_list", "File with a list of image pair indices to match", ArgvParser::OptionRequired);
    cmd.defineOption("keyfile_list", "List of key files (full paths) in LOWE's" 
            "format (ASCII text or gzipped)", ArgvParser::OptionRequired);

    cmd.defineOption("matches_file", "Filename with path, file stores " 
            "key matches in format <im1 im2\\n num_matches\\n keyIdx1 keyIdx2\\n...", 
            ArgvParser::OptionRequired);

    cmd.defineOption("topscale_percent", "Percentage of top scale features"
            "to use for matching, [Default: 10]", ArgvParser::NoOptionAttribute); 

    /// If instead of arguments, options file is supplied
    /// Parse options file to fill-up a dummy argv struct
    /// Parse the dummy argv struct to get true arguments
    vector< char* > argstr;
    bool optionsFile = false;

    if(argc == 2) {
        string str( argv[1] );

        /// Find position of = to separate option & value
        int pos = str.find("=");
        string optStr = str.substr(0,pos); //option string

        /// Check if the passed option is that of options file
        if(optStr == "-o" || optStr == "--options_file") {

            string optFileName = str.substr(pos+1, str.length());
            std::ifstream optFile(optFileName.c_str());
            if(!optFile.is_open()) {
                cout << "\nError opening options file " << optFileName; 
                exit(-1);
            }

            /// Fill argstr with text from options file
            std::string option(argv[0]);  
            do {
                char *arg = new char[option.size() + 1];
                copy(option.begin(), option.end(), arg);
                arg[option.size()] = '\0';
                argstr.push_back(arg);
            } while(std::getline(optFile, option));
            optionsFile = true;
        }
    } 

    /// finally parse and handle return codes (display help etc...)

    int result = -1;
    if(optionsFile) {
        result = cmd.parse(argstr.size(), argstr.data());
    } else {
        result = cmd.parse(argc, argv);
    }

    if (result != ArgvParser::NoParserError)
    {
        printf("\nProblem is here");
        fflush(stdout);
        cout << cmd.parseErrorDescription(result);
        exit(-1);
    }
}

void DeleteMultiscaleKdTree( vector< ANNkd_tree*>& trees) {
    for(int i=0; i < trees.size(); i++) {
        annDeallocPts( trees[i]->pts );
        delete trees[i];
    }
}

void CreateMultiscaleKdTree(vector<ANNkd_tree*>& trees, 
        unsigned char* keys, 
        int numKeys, int limit = 5, int numPartition = 10) {


    int partitionSize = (int)(floor(numKeys/numPartition));
    if(partitionSize < 1000) {
        partitionSize = 1000; 
        if( limit*partitionSize > numKeys ) {
            limit = (int)(numKeys/partitionSize);
            //This is for images with fewer than 1000 features
            if(numKeys < partitionSize) {
                partitionSize = numKeys;
                limit = 1;
            }
        }
    }

    vector < ANNpointArray > pts( limit, NULL );
    trees.resize(limit);

    for(int i=0; i < limit; i++) {
        int first = i*partitionSize;
        int last = (i+1)*partitionSize - 1;

        pts[i] = annAllocPts( partitionSize, 128 );
        for(int f=first; f < last; f++) {
            memcpy( pts[i][f-first], keys+128*f, 
                    sizeof(unsigned char)*128);
        }
        trees[i] = new ANNkd_tree( pts[i], partitionSize, 128, 16 );
    }
}

vector< int > ComputeTreeIndices(bool newPair, int numTrees,  
        int matchedPartition = -1) {
    static vector< pair<int, int> > scoreIdxPairs; 
    static vector<int> treeRanks; 
    if(treeRanks.empty() && scoreIdxPairs.empty()) {
        for(int i=0; i < numTrees; i++) {
            treeRanks.push_back(i);
            scoreIdxPairs.push_back(make_pair(0,numTrees-i-1));
        }
    } 

    if(newPair) {
        treeRanks.clear();
        scoreIdxPairs.clear();
        for(int i=0; i < numTrees; i++) {
            treeRanks.push_back(i);
            scoreIdxPairs.push_back(make_pair(0,numTrees-i-1));
        }
    }

    if(matchedPartition != -1) {
        for(int i=0; i < numTrees; i++) {
            if(scoreIdxPairs[i].second == numTrees - matchedPartition - 1) {
                scoreIdxPairs[i].first++;
            } 
        }
        sort(scoreIdxPairs.begin(), scoreIdxPairs.end(), std::greater<pair<int,int> >());
        for(int i=0; i < numTrees; i++) {
            treeRanks[i] = - scoreIdxPairs[i].second + numTrees - 1;
        }
    }
    return treeRanks;
}

int SearchInMultiscaleKdtree( bool newPair, unsigned char* query, 
        vector< ANNkd_tree* > trees, int numFeatures) {

    static int count0, count1;
    if(newPair) {
        if(count0 < 16 && count1 > 16) {
            printf("\nAdded new image-pair due to fine logic");
        }
        count0 = 0;
        count1 = 0;
    }
    float prevBest = 1000000, prevSecondBest = 1000000;

    vector<int> treeIds = ComputeTreeIndices(newPair, trees.size());
    int numTrees = treeIds.size();

    int matchingPt = -1;

    for(int i=0; i < numTrees; i++) {
        vector <ANNidx> indices(2); 
        vector <ANNidx> dists(2); 

        int idx = treeIds[i];
        trees[idx]->annkPriSearch( query, 2, indices.data(), 
                dists.data(), 0.0);

        float bestDist = (float)(dists[0]);
        float secondBestDist = (float)(dists[1]);

        if(bestDist >= prevBest) {
            if( bestDist < prevSecondBest ) {
                prevSecondBest = bestDist;
            }
            continue;
        }

        if( secondBestDist > prevBest ) {
            secondBestDist = prevBest;
        }

        float distRatio = bestDist/secondBestDist;
        int partitionSize = (int)(floor(numFeatures/10));
        if(partitionSize < 1000) {
            partitionSize = 1000;
            /* This is probably not necessary as idx will always be 0 when this happens */
            if(numFeatures < 1000) {
                partitionSize = numFeatures;
            }
        }

        if(distRatio <= 0.6*0.6) {
            if(idx != 0) {
                count1++;
            }
            if(idx == 0) {
                count0++;
            }
            int relIdx = (int)indices[0];
            matchingPt = relIdx + partitionSize*idx;
            ComputeTreeIndices(false, trees.size(), idx);
            break;
        } 
    }
    return matchingPt;
}

int main(int argc, char* argv[]) {
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string pairsList = cmd.optionValue("pairs_list");
    string keyList = cmd.optionValue("keyfile_list");
    string matchFileName = cmd.optionValue("matches_file");
    string rankFileName = cmd.optionValue("ranks_list");

    int topscale = 10;
    if(cmd.foundOption("topscale_percent")) {

        string str = cmd.optionValue("topscale_percent");
        topscale = atoi(str.c_str());
    }

    clock_t start = clock();
    ifstream pairsFile(pairsList.c_str());
    if(!pairsFile.is_open()) {
        cout << "\nError opening pairs file";
        return -1;
    }

    ifstream keyFile(keyList.c_str());
    if(!keyFile.is_open()) {
        cout << "\nError Opening File";
        return -1;
    }

    vector< pair<int, int> > candidatePairs;
    string line;
    set<int> imagesLeft;
    set<int> imagesRight;
    while(getline(pairsFile, line)) {
        int first, second;
        istringstream pairStr(line);
        pairStr >> first >> second;
        candidatePairs.push_back(make_pair( first, second) );

        imagesLeft.insert( first );
        imagesRight.insert( second );
    }

    sort(candidatePairs.begin(), candidatePairs.end());

    vector<string> keyFileNames;
    while(getline(keyFile, line)) {
        keyFileNames.push_back(line);
    }

    bool leftRef = false;
    int numKeys = 0;
    int numTrees = imagesRight.size();
    if(imagesLeft.size() < imagesRight.size()) {
        leftRef = true;
        numTrees = imagesLeft.size();
    }

    numKeys = imagesLeft.size() + imagesRight.size();


    vector< unsigned char* > keys(numKeys);
    vector< keypt_t* > keysInfo(numKeys);
    vector< int > numFeatures(numKeys);

    vector< vector< ANNkd_tree* > > refTrees(numTrees);

    int i;
    set<int>::iterator it;
    map<int,int> indexMap, treeIndex;


    printf("\nReading Left Key Files ");
    fflush(stdout);
    for(it = imagesLeft.begin(), i = 0; it != imagesLeft.end(); it++, i++) {
        printf(" %d", i);
        fflush(stdout);
        indexMap.insert(make_pair(*it,i));
        numFeatures[i] = ReadKeyFile(keyFileNames[(*it)].c_str(), &keys[i], &keysInfo[i]);
    }
    
    printf("\nReading Right Key Files ");
    fflush(stdout);
    for(it = imagesRight.begin(); it != imagesRight.end(); it++, i++) {
        printf(" %d", i);
        fflush(stdout);
        indexMap.insert(make_pair(*it,i));
        numFeatures[i] = ReadKeyFile(keyFileNames[(*it)].c_str(), &keys[i], &keysInfo[i]);
    }
    
    set<int>& refImagesList = leftRef ? imagesLeft : imagesRight;
    i = leftRef ? 0 : imagesLeft.size();

    int count = 0;
    for(it = refImagesList.begin(); it != refImagesList.end(); it++, i++, count++) {
        CreateMultiscaleKdTree(refTrees[count], keys[i], numFeatures[i]);
        treeIndex.insert(make_pair(*it, count));
    }


    ofstream matchFile( matchFileName.c_str(), std::ofstream::out);
    if(!matchFile.is_open()) {
        cout << "\nError opening match file";
        return -1;
    }

    ofstream rankFile( rankFileName.c_str(), std::ofstream::out);
    if(!rankFile.is_open()) {
        cout << "\nError opening rank file";
        return -1;
    }
    
    clock_t end = clock();    
    printf("[KeyMatchGeoAware] Reading keys took %0.3fs\n", 
            (end - start) / ((double) CLOCKS_PER_SEC));


    count = 0;
    for(int i=0; i < candidatePairs.size(); i++) {
        int srcIdx = -1, refIdx = -1; 
        if(!leftRef) {
            srcIdx = candidatePairs[i].first;
            refIdx = candidatePairs[i].second;
        } else {
            srcIdx = candidatePairs[i].second;
            refIdx = candidatePairs[i].first;
        }

        printf("[KeyMatchGeoAware] Matching image %d - %d\n", srcIdx, refIdx);
        fflush(stdout);
        start = clock();

        bool newPair = true;


        int index = indexMap[ srcIdx ];
        unsigned char* srcKeys = keys[ index ];

        bool allQueriesDone = false;
        int numQueries = (int)(numFeatures[index]*topscale/100);
        if(numFeatures[index] < 1000) {
            numQueries = numFeatures[index];
            allQueriesDone = true;
        } 

        annMaxPtsVisit(100);

        map< int, int > matchesMap;
        vector< pair<int,int> > matches;

        int treeIdx = treeIndex[ refIdx ];
        for(int q = 0; q < numQueries; q++) {
            if(q != 0) newPair = false;
            unsigned char* qKey = srcKeys + 128*q;

            int matchingPt = SearchInMultiscaleKdtree( newPair, 
                    qKey, refTrees[treeIdx], numFeatures[treeIdx]); 
            if(matchingPt != -1) {
                pair< map<int, int>::iterator, bool> ret;
                ret = matchesMap.insert( make_pair(matchingPt, q ) );
                if(ret.second) {
                    matches.push_back( make_pair( q, matchingPt ) );
                }
            }
        }

        int numMatches = matches.size();

        if(numMatches >= 4 && numMatches < 16 && !allQueriesDone) {
            for(int q=numQueries; q < 2*numQueries; q++) {
                unsigned char* qKey = srcKeys + 128*q;

                int matchingPt = SearchInMultiscaleKdtree( newPair, 
                        qKey, refTrees[treeIdx], numFeatures[treeIdx]); 
                if(matchingPt != -1) {
                    pair< map<int, int>::iterator, bool> ret;
                    ret = matchesMap.insert( make_pair(matchingPt, q ) );
                    if(ret.second) {
                        matches.push_back( make_pair( q, matchingPt ) );
                    }
                }
            }
            numMatches = matches.size();
            printf("\nConsidered a new pair for 20p matching");
            if(numMatches > 16) {
                count++;
            }
        }

        if(numMatches > 0) {
            rankFile << srcIdx << " " << refIdx << " " << numMatches << endl;
            rankFile << refIdx << " " << srcIdx << " " << numMatches << endl;
        }

        if(numMatches >= 16) {
            printf("Writing %d matches between images %d and %d\n", 
                    numMatches, srcIdx, refIdx);
            matchFile << srcIdx << " " << refIdx << endl;
            matchFile << numMatches << endl;
            for(int m=0; m < matches.size(); m++) {
                matchFile << matches[m].first 
                    << " " << matches[m].second << endl;
            }
        }

        end = clock();    
        printf("[KeyMatchGlobal] Matching took %0.3fs\n", 
                (end - start) / ((double) CLOCKS_PER_SEC));
        fflush(stdout);
    }
    matchFile.close();
    rankFile.close();

    printf("\nAdded %d new pairs due to adaptive queries", count);
    /// Skiped Freeing keyfile memory due to performance issues
    /// Assuming program exits right afterwards
    /// Please free it if you intend to extend this code beyond this point

    // for(int i=0; i < numKeys; i++) {
    //  delete[] keys[i];
    //  delete[] keysInfo[i];
    //}

    return 0;
}
