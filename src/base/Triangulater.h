#ifndef __TRIANGULATER_H
// Include files from Bundler libraries
#include "matrix.h"
#include "triangulate.h"
#include "fmatrix.h"
#include "vector.h"

// Include files from base directory
#include "../base/Geometric.h"
#include "../base/Bundle.h"
#include "../base/Reader.h"

using namespace std;

namespace triang {

  bool RemoveBadPointsAndCameras(bundle::Bundle* bdl, v3_t point, 
        vector<int>& pt_views);

  void InvertDistortion(int n_in, int n_out, double r0, double r1,
        double *k_in, double *k_out);

  v2_t sfm_project_final(double* R, double* t, double f, double* k, v3_t pt, 
        int explicit_camera_centers, int undistort);

  bool CheckCheirality(v3_t p, double* R, double* t);

  v2_t UndistortNormalizedPoint(v2_t p, double* kinv);


  bool TriangulateTrack(bundle::Bundle* bdl, reader::ImageListReader& imList, 
        vector<pointstr> &trackDetails, v3_t& ptOut, bool angleVerify); 

};
#endif // __TRIANGULATER_H
