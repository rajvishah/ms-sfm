#ifndef __LOCALIZE_H
#define __LOCALIZE_H 

#include "sfm.h"
#include "Bundle.h"
#include "ImageData.h"

#define NUM_ERROR_BINS 10
#define NUM_STDDEV 2.0 // 3.0 // 6.0
#define MIN_INLIERS_EST_PROJECTION 6 /* 7 */ /* 30 */ /* This constant needs adjustment */


bool BundleRegisterImage(localize::ImageData& data,vector<v3_t>& pt3,vector<v2_t>& pt2,
        vector<int>& pt3_idx,vector<int>& pt2_idx);
void SetFocalConstraint(const localize::ImageData& data,camera_params_t *params);
void InitializeCameraParams(const localize::ImageData& data,camera_params_t& camera);
void ClearCameraConstraints(camera_params_t *params);
void FixIntrinsics(double *P,double *K,double *R,double *t);

#endif //__LOCALIZE_H 
