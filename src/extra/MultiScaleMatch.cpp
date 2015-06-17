#include <iostream>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include "keys2a.h"
#include "argvparser.h"
using namespace std;
using namespace CommandLineProcessing;


void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
  cmd.setIntroductoryDescription("Pair-wise feature matching example");
  //define error codes
  cmd.addErrorCode(0, "Success");
  cmd.addErrorCode(1, "Error");

  cmd.setHelpOption("h", "help",""); 
  cmd.defineOption("source_image", "<Path to Source Image File>", 
      ArgvParser::OptionRequired);
                  
  cmd.defineOption("target_image", "<Path to Target Image File>", ArgvParser::OptionRequired);


  cmd.defineOption("source_key","<Path to Source Key File>",ArgvParser::OptionRequired); 

  cmd.defineOption("target_key", "<Path to Target Key File>", ArgvParser::OptionRequired);
                    
  cmd.defineOption("result_path", "<Path to save results>", ArgvParser::NoOptionAttribute);

  /// finally parse and handle return codes (display help etc...)

  int result = -1;
  result = cmd.parse(argc, argv);

  if (result != ArgvParser::NoParserError)
  {
    cout << cmd.parseErrorDescription(result);
    exit(1);
  }

}

bool multiScaleMatch(ArgvParser& cmd) {
  string imagePath1  = cmd.optionValue("source_image");
  string imagePath2  = cmd.optionValue("target_image");

  string keyPath1  = cmd.optionValue("source_key");
  string keyPath2  = cmd.optionValue("target_key");
  
  unsigned char* queryKey;
  unsigned char* refKey;
  keypt_t* queryKeyInfo;
  keypt_t* refKeyInfo;

  int nPts1 = ReadKeyFile(keyPath1.c_str(),
      &queryKey, &queryKeyInfo);

  int nPts2 = ReadKeyFile(keyPath2.c_str(),
      &refKey, &refKeyInfo);


  vector< ANNpointArray > ptArr(10, NULL);
  vector< ANNkd_tree* > trees(10, NULL);

  vector< pair < int, int > > scoreIndexPairs;

  int segmentSize = (int)(nPts2/10);
  for(int i=0; i < 10; i++) {
    int first = (int)(i*segmentSize);
    int last = first;
    if(i < 9) {
      last = (int)((i+1)*segmentSize)-1;
    } else {
      last = nPts2;
    }

    int numPts = last - first + 1;
    ptArr[i] = annAllocPts( numPts, 128);
      
    for(int f=first; f < last; f++) {
      memcpy( ptArr[i][f-first], refKey+128*f, sizeof(unsigned char)*128 );
    }

    trees[i] = new ANNkd_tree( ptArr[i], numPts, 128, 16 );

    scoreIndexPairs.push_back(make_pair(0, i));
  }
  annMaxPtsVisit(0);

  int numQueries = (int)(nPts1/10);
  

  /*
  ANNpointArray refPtsArr = annAllocPts( nPts2, 128 );
  for(int i=0; i < nPts2; i++) {
    memcpy( refPtsArr[i], refKey+128*i, sizeof(unsigned char)*128 );
  }
  ANNkd_tree* fullRefTree = new ANNkd_tree( refPtsArr, nPts2, 128, 16);

  vector < pair< int, int > > fullMatches;
  for(int i = 0; i < numQueries; i++) {

    unsigned char* qKey = queryKey + 128*i;
    vector<ANNidx> indices(2);
    vector<ANNdist> dists(2);

    fullRefTree->annkPriSearch( qKey, 2, indices.data(), dists.data(), 0.0 );

    float bestDist = (float)(dists[0]);
    float secondBestDist = (float)(dists[1]);
    float distRatio = sqrt(bestDist/secondBestDist);
    
    if(distRatio > 0.6) {
      continue;
    }
    int matchingPt = (int)indices[0];
    fullMatches.push_back( make_pair( i, matchingPt ) );

  }

  annMaxPtsVisit(0);
  */
  vector < pair< int, int > > matches;
  vector < int > scores(10, 0);

  for(int i = 0; i < numQueries; i++) {

    unsigned char* qKey = queryKey + 128*i;

    float prevBest = 1000000;
    float prevSecondBest = 1000000;
    for(int j = 0; j < 5; j ++) {
      vector<ANNidx> indices(2);
      vector<ANNdist> dists(2);
      int treeIdx = scoreIndexPairs[j].second;
      trees[treeIdx]->annkPriSearch( qKey, 2, indices.data(), 
          dists.data(), 0.0 );

      /// Compute best distance to second best distance ratio
      float bestDist = (float)(dists[0]);
      float secondBestDist = (float)(dists[1]);

      if(bestDist > prevBest) {
        if(bestDist < prevSecondBest) {
          prevSecondBest = bestDist;
        }
        continue; 
      } else {
        if(prevBest < secondBestDist) {
          secondBestDist = prevBest;
        }  
      }

      float distRatio = sqrt(bestDist/secondBestDist);

      if(distRatio <= 0.6) {
        int matchingPt = (int)indices[0];
        int secondMatch = (int)indices[1];

        vector< pair<int,int> > :: iterator itr;
        scores[treeIdx]++;
        itr = find(scoreIndexPairs.begin(), scoreIndexPairs.end(), make_pair( scores[treeIdx]-1, treeIdx ) );
        (*itr).first++;

        matches.push_back( make_pair( i, matchingPt ) );
        break;
      }

      prevBest = bestDist;
      prevSecondBest = secondBestDist;
    }
  }

  printf("\nNumber of Points %d - %d , Matches : %d", numQueries, nPts2, matches.size() );
  //printf("\nNumber of Points %d - %d , Full Matches : %d", numQueries, nPts2, fullMatches.size() );
  for(int i=0; i < 10; i++) {
    printf("\nMatches in Kd tree %d - %d", i, scores[i] );
    annDeallocPts( ptArr[i] );
    delete trees[i];
  }
  
  return true;
}

int main( int argc, char* argv[] ) {
  ArgvParser cmd;
  SetupCommandlineParser(cmd, argc, argv);
  bool status = multiScaleMatch(cmd);
  return 0;
}
