/* 
 *  Copyright (c) 2008-2010  Noah Snavely (snavely (at) cs.cornell.edu)
 *    and the University of Washington
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

/* KeyMatchFull.cpp */
/* Read in keys, match, write results to a file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <assert.h>
#include <time.h>
#include <set>
#include <algorithm>

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



int main(int argc, char **argv) {

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

    vector< ANNkd_tree* > refTrees(numTrees);

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

    printf("\nDone reading keyfiles");

    set<int>& refImagesList = leftRef ? imagesLeft : imagesRight;
    i = leftRef ? 0 : imagesLeft.size();

    int count = 0;
    for(it = refImagesList.begin(); it != refImagesList.end(); it++, i++, count++) {
        printf("\nBuilding Tree %d ...", count);
        fflush(stdout);
        int numTreeEle = (int)(numFeatures[i]*topscale/100);

        /*
        if(numFeatures[i] < 1000) {
            printf("\nLow feature points");
            fflush(stdout);
            numTreeEle = numFeatures[i];
        } 
        */
        refTrees[count] = CreateSearchTree( numTreeEle, keys[i]);
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
    printf("[KeyMatchRegular] Reading keys took %0.3fs\n", 
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

        printf("[KeyMatchRegular] Matching image %d - %d\n", srcIdx, refIdx);
        fflush(stdout);
        start = clock();
        bool newPair = true;
        int index = indexMap[ srcIdx ];
        unsigned char* srcKeys = keys[ index ];

        bool allQueriesDone = false;
        int numQueries = (int)(numFeatures[index]*topscale/100);
        /*
        if(numFeatures[index] < 1000) {
            numQueries = numFeatures[index];
            allQueriesDone = true;
        } 
        */

        int treeIdx = treeIndex[ refIdx ];

        vector<KeypointMatch> matches = MatchKeys(numQueries, srcKeys, refTrees[treeIdx], 0.6);

        int numMatches = (int) matches.size();
        if(numMatches >= 16) {
            printf("Writing %d matches between images %d and %d\n", 
                    numMatches, srcIdx, refIdx);
            matchFile << srcIdx << " " << refIdx << endl;
            matchFile << numMatches << endl;
            for(int m=0; m < matches.size(); m++) {
                matchFile << matches[m].m_idx1 
                    << " " << matches[m].m_idx2 << endl;
            }
        }
        end = clock();    
        printf("[KeyMatchGlobal] Matching took %0.3fs\n", 
                (end - start) / ((double) CLOCKS_PER_SEC));
        fflush(stdout);
    }
    matchFile.close();
    rankFile.close();

    printf("\n[GlobalMatchLists] Done matching pairs in list %s", pairsList.c_str());
    /// Skiped Freeing keyfile memory due to performance issues
    /// Assuming program exits right afterwards
    /// Please free it if you intend to extend this code beyond this point

    // for(int i=0; i < numKeys; i++) {
    //  delete[] keys[i];
    //  delete[] keysInfo[i];
    //}

    return 0;
}

