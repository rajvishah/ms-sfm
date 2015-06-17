#ifndef __MATCHPAIRS_H
#define __MATCHPAIRS_H

#include "Reader.h"
#include "Bundle.h"

namespace sfm{
class MatchPairs {
    bool initialized;
	reader::BundleReader br;
    reader::ImageListReader imgList;
    reader::KeyListReader keyList;
    bundle::Bundle bdl;

    vector<int> localizedImages;
    vector<int> query2locIdx;
    vector< vector <int> > candidateLists;
    vector< vector <float> > candidateScores;
    vector< pair<int, int> > candidatePairs; 
    vector< vector<double> > candidatePairsF; 
    vector< vector<double> > candidatePairsFT; 

    int numMaxCandidates;
    int numMinCovisible;
    int totalCandidatePairs;


  bool twoLevelNearbySearch(int qImgId,
          map<int, vector<pair<int, int> > >& hist,
          vector< int >& sortedList);

  void createPointHistogram(
          vector<pair< int, int > >& list, 
          map<int, vector<pair<int, int> > >& hist);

  int sortAndTruncateHistogram(
          map<int, vector<pair< int, int> > >& hist, 
          vector<int>& sortedList, int maxLim, int queryId);

  bool getCandidatePoints(
          pair<const int, vector< pair<int, int> > >& ptList,
          vector< pair<int, int> >& candidates);

    public:

    int getTotalCandidatePairs() {
        return totalCandidatePairs;
    }

    bool isInitialized() {
        return initialized;
    }

    MatchPairs(string& param, int nearby);


    const vector<int>& getLocalizedImages() {
        return localizedImages;   
    }

    const vector<int>& getCandidateImages(int query) {
            return candidateLists[query];
    }

    const vector< pair<int, int> >& getMatchPairs(){
        return candidatePairs;
    }


    int getTotalPairs() {
        int n = localizedImages.size();
        return (int)(n*(n-1)/2);
    }

    bool matchListPairs(string& listPath ,string& resultDir);
    bool writeUniquePairs(string& resultDir, int numLists); 
    void findAllCandidateImages(bool tryHard=false);
    bool findSelectedCandidateImages(string& queryFile, bool tryHard=false);
    bool findCandidateImages(int query, bool tryHard=false);
    int findUniquePairs();
    bool matchAllPairs(string& output);
};
}
#endif //__MATCHPAIRS_H 
