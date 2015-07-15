#ifndef __LOCALIZE_H
#define __LOCALIZE_H 

#include "sfm.h"
#include "Bundle.h"
#include "ImageData.h"

#define NUM_ERROR_BINS 10
#define NUM_STDDEV 2.0 // 3.0 // 6.0
#define MIN_INLIERS_EST_PROJECTION 6 /* 7 */ /* 30 */ /* This constant needs adjustment */


bool BundleRegisterImage(localize::ImageData& data,vector<v3_t>& pt3,vector<v2_t>& pt2,
        vector<int>& pt3_idx,vector<int>& pt2_idx, double* P, double* K, double* R, double*t);
void SetFocalConstraint(const localize::ImageData& data,camera_params_t *params);
void InitializeCameraParams(const localize::ImageData& data,camera_params_t& camera);
void ClearCameraConstraints(camera_params_t *params);
void FixIntrinsics(double *P,double *K,double *R,double *t);
bool FindAndVerifyCamera(int num_points, v3_t *points_solve, v2_t *projs_solve,
                         int *idxs_solve, double* P,
                         double *K, double *R, double *t, 
                         double proj_estimation_threshold,
                         double proj_estimation_threshold_weak,
                         std::vector<int> &inliers,
                         std::vector<int> &inliers_weak,
                         std::vector<int> &outliers);

bool ConvertCamera(int num_points, v3_t *points_solve, v2_t *projs_solve,
                    double* P, double *Kinit, double *Rinit, double *tinit, 
                    double proj_estimation_threshold,
                    double proj_estimation_threshold_weak,
                    std::vector<int> &inliers,
                    std::vector<int> &inliers_weak,
                    std::vector<int> &outliers);
#endif //__LOCALIZE_H 
