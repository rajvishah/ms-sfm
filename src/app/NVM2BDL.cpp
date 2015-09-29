#include <fstream>
#include <sstream>
#include "../base/Reader.h"
#include "../base/Bundle.h"

int main(int argc, char* argv[]) {

  string bundleFileName = argv[1];
  string mapFileName1 = argv[2];
  string mapFileName2 = argv[3];
  string outputFileName = argv[4];

  ifstream mapFile1( mapFileName1.c_str(), ifstream::in );
  if(!mapFile1.is_open()) {
    printf("\nCould not open map file 1");
    return -1;
  }

  ifstream mapFile2( mapFileName2.c_str(), ifstream::in );
  if(!mapFile2.is_open()) {
    printf("\nCould not open map file 1");
    return -1;
  }

  map<int,int> idxMap1;
  string line;
  while(getline(mapFile1, line)) {
    istringstream str(line);
    int a, b;
    string name;

    str >> a >> b >> name;
    idxMap1[a] = b; 
  }

  map<int,int> idxMap2;
  while(getline(mapFile2, line)) {
    istringstream str(line);
    int a, b;
    string name;

    str >> a >> b >> name;
    idxMap2[a] = b; 
  }

  int numTotalImages = idxMap2.size();
  vector< vector<double> > intrinsics(numTotalImages);
  vector< vector<double> > rotations(numTotalImages);
  vector< vector<double> > translations(numTotalImages);
  vector< bool > validImage(numTotalImages, false);

  for(int i=0; i < numTotalImages; i++) {
    intrinsics[i].resize(3,0.0); 
    rotations[i].resize(9,0.0); 
    translations[i].resize(3,0.0); 
    validImage[i] = false;
  }

  ifstream infile( bundleFileName.c_str(), ifstream::in);
  if(infile.is_open() != true) {
		return false; 
  }

  string header;
  getline(infile, header);

  int numImgs, numPoints;
  getline(infile, line);
  stringstream strm(line);

  strm >> numImgs >> numPoints;
  
  vector< vector<float> > coords(numPoints);
  vector< vector<int> > colors(numPoints);
  vector< vector<pointstr> > tracks(numPoints);

  for(int i=0; i < numImgs; i++) {
    int camIdx = idxMap1[i];
  //  printf("\nImage %d: %d: ", i, camIdx);
      fflush(stdout);
    if(camIdx >= numTotalImages) {
      printf("\nBig Problem");
      fflush(stdout);
    }
    validImage[camIdx] = true;

    getline(infile, line);
    stringstream strm(line);
    for(int c=0; c < 3; c++) {
      strm >> intrinsics[camIdx][c];  
    }

    for(int c=0; c < 3; c++) {
      getline(infile, line);
      stringstream strm1(line);
      strm1 >> rotations[camIdx][c*3 + 0];  
      strm1 >> rotations[camIdx][c*3 + 1];  
      strm1 >> rotations[camIdx][c*3 + 2];  
    }
    
    getline(infile, line);
    stringstream strm1(line);
    for(int c=0; c < 3; c++) {
      strm1 >> translations[camIdx][c];  
    }
   
    /*
    for(int m=0; m < 3; m++) {
      printf("%lf ", intrinsics[camIdx][m]);
    }
    for(int m=0; m < 9; m++) {
      printf("%lf ", rotations[camIdx][m]);
    }
    for(int m=0; m < 3; m++) {
      printf("%lf ", translations[camIdx][m]);
    }
    */

    fflush(stdout);
  }

  for(int i=0; i < numPoints; i++) {

    coords[i].resize(3,0.0);
    colors[i].resize(3,0.0);

    getline(infile, line);
    stringstream strm(line);
    strm >> coords[i][0] >> coords[i][1] >> coords[i][2];

    getline(infile, line);
    stringstream strm1(line);
    strm1 >> colors[i][0] >> colors[i][1] >> colors[i][2];
		
    getline(infile, line);
    stringstream strm2(line);

    int num_visible;
    strm2 >> num_visible;
    
    tracks[i].resize(num_visible);
    for (int j = 0; j < num_visible; j++) {
			int view, key;
			double x, y;
      strm2 >> view >> key >> x >> y;

      tracks[i][j].viewId = idxMap1[view];
      tracks[i][j].siftId = key;
      tracks[i][j].xcord = x;
      tracks[i][j].ycord = y;
    }  
  }


  printf("\nTotal images are %d", numTotalImages);
  FILE* fp = fopen(outputFileName.c_str(),"w");
  if(fp == NULL) {
    printf("\nCould not open output file");
    return -1;
  }

  fprintf(fp, "%s\n", header.c_str());
  fprintf(fp, "%d %d\n", numTotalImages, numPoints);
  for(int i=0; i < numTotalImages; i++) {
    if(validImage[i] == true) {
      fprintf(fp, "%lf %lf %lf\n", intrinsics[i][0], intrinsics[i][1], intrinsics[i][2]);
      for(int r = 0; r < 3; r++) {
        fprintf(fp, "%lf %lf %lf\n", rotations[i][r*3 + 0], 
            rotations[i][r*3 + 1], rotations[i][r*3 + 2]);
      }
      fprintf(fp, "%lf %lf %lf\n", translations[i][0], translations[i][1], translations[i][2]);
    } else {
      fprintf(fp,"0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n");
    }
  }

  for(int i=0; i < tracks.size(); i++) {
    fprintf(fp, "%lf %lf %lf\n", coords[i][0], coords[i][1], coords[i][2]);
    fprintf(fp, "%d %d %d\n", colors[i][0], colors[i][1], colors[i][2]);
    fprintf(fp, "%d", tracks[i].size());
    for(int j=0; j < tracks[i].size(); j++) {
      pointstr p = tracks[i][j];
      fprintf(fp," %d %d %lf %lf", p.viewId, p.siftId, p.xcord, p.ycord);
    }
    fprintf(fp,"\n");
  }

  fclose(fp);

  return -1;
}

