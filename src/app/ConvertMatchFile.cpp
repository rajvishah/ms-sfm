#include <iostream>
#include <fstream>

#include "../base/Reader.h"
#include "../base/keys2a.h"

using namespace std;

int main(int argc, char* argv[]) {
  string keyListFile = argv[1];
  string imgListFile = argv[1];
  string matchesFile = string(argv[2]) + "/matches.init.txt";
  string outFile = string(argv[2]) + "/matches.txt";
  
  reader::ImageListReader imgList(imgListFile);
  reader::KeyListReader keyList(keyListFile);
  
  bool s1 = imgList.read();
  bool s2 = keyList.read();

  if(!s1 || !s2) {
    printf("\nError reading list file or key file");
    return 0;
  }
  
  // Initialize containers for various data
  int numKeys = keyList.getNumKeys();
  vector< unsigned char* > keys(numKeys);
  vector< keypt_t* > keysInfo(numKeys);
  vector< int > numFeatures(numKeys);


  for(int i=0; i < keyList.getNumKeys(); i++) {
    printf("\nReading keyfile %d/%d", i, numKeys);
    string keyPath = keyList.getKeyName(i);
    numFeatures[i] = ReadKeyFile(keyPath.c_str(),
        &keys[i], &keysInfo[i]);
    int W = imgList.getImageWidth(i);
    int H = imgList.getImageHeight(i);
    NormalizeKeyPoints(numFeatures[i], keysInfo[i], W, H);
  }
  
  FILE* fp = fopen( matchesFile.c_str() , "r");
  if( fp == NULL ) {
    printf("\nCould not read matches file");
    fflush(stdout);
    return 0;
  }
  
  FILE* fw = fopen( outFile.c_str() , "w");
  if( fw == NULL ) {
    printf("\nCould not write matches file");
    fflush(stdout);
    return 0;
  }

  int img1, img2;
  while(fscanf(fp,"%d %d",&img1,&img2) != EOF) {

    int numMatches;
    fscanf(fp,"%d",&numMatches);
    
    for(int i=0; i < numMatches; i++) {
      int matchIdx1, matchIdx2;
      fscanf(fp,"%d %d",&matchIdx1, &matchIdx2);

      float x1 = keysInfo[img1][matchIdx1].nx;
      float y1 = keysInfo[img1][matchIdx1].ny; 

      float x2 = keysInfo[img2][matchIdx2].nx; 
      float y2 = keysInfo[img2][matchIdx2].ny;

      fprintf(fw, "%d %d %lf %lf %d %d %lf %lf\n", img1, matchIdx1, x1, y1, img2, matchIdx2, x2, y2);
    }
  }
  fclose(fp);
  fclose(fw);
  
  return 0;
}
