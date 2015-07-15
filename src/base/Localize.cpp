#include "sfm.h"
#include "matrix.h"
#include "qsort.h"
#include "util.h"
#include "triangulate.h"
#include "defines.h"
#include "ImageData.h"
#include "Localize.h"

#include "defs.h"
static int compare_doubles(const void *d1, const void *d2)
{
    double a = *(double *) d1;
    double b = *(double *) d2;

    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

void FixIntrinsicsRajvi(double *Kinit, double *Rinit, double *tinit) 
{
    int neg = (Kinit[0] < 0.0) + (Kinit[4] < 0.0) + (Kinit[8] < 0.0);
    for(int i=0; i < 3; i++) {
        int idx = i*3 + i;
        if(Kinit[idx] < 0.0) {
            Kinit[idx] *= -1.0f;
            Rinit[i*3] *= -1.0f;
            Rinit[i*3+1] *= -1.0f;
            Rinit[i*3+2] *= -1.0f;
            tinit[i] *= -1.0f;
        }
    }
}

/* Use a 180 rotation to fix up the intrinsic matrix */
void FixIntrinsics(double *P, double *K, double *R, double *t) 
{
    /* Check the parity along the diagonal */
    int neg = (K[0] < 0.0) + (K[4] < 0.0) + (K[8] < 0.0);

    /* If odd parity, negate the instrinsic matrix */
    if ((neg % 2) == 1) {
        matrix_scale(3, 3, K, -1.0, K);
        matrix_scale(3, 4, P, -1.0, P);
    }

    /* Now deal with case of even parity */
    double fix[9];
    matrix_ident(3, fix);
    double tmp[9], tmp2[12];

    if (K[0] < 0.0 && K[4] < 0.0) {
        fix[0] = -1.0;
        fix[4] = -1.0;
    } else if (K[0] < 0.0) {
        fix[0] = -1.0;
        fix[8] = -1.0;
    } else if (K[4] < 0.0) {
        fix[4] = -1.0;
        fix[8] = -1.0;
    } else {
        /* No change needed */
    }

    matrix_product(3, 3, 3, 3, K, fix, tmp);
    memcpy(K, tmp, sizeof(double) * 3 * 3);

    double Kinv[9];
    matrix_invert(3, K, Kinv);

    matrix_product(3, 3, 3, 4, Kinv, P, tmp2);

    memcpy(R + 0, tmp2 + 0, sizeof(double) * 3);
    memcpy(R + 3, tmp2 + 4, sizeof(double) * 3);
    memcpy(R + 6, tmp2 + 8, sizeof(double) * 3);

    t[0] = tmp2[3];
    t[1] = tmp2[7];
    t[2] = tmp2[11];
}

void ClearCameraConstraints(camera_params_t *params) 
{
    for (int i = 0; i < NUM_CAMERA_PARAMS; i++) {
        params->constrained[i] = false;
        params->constraints[i] = 0.0;
        params->weights[i] = 0.0;
    }
}

void InitializeCameraParams(const localize::ImageData& data, camera_params_t& camera)
{
    matrix_ident(3, camera.R);
    camera.t[0] = camera.t[1] = camera.t[2] = 0.0;
    camera.f = 0.0;
    camera.k[0] = camera.k[1] = 0.0;

    camera.k_inv[0] = camera.k_inv[2] = camera.k_inv[3] = 0.0;
    camera.k_inv[4] = camera.k_inv[5] = 0.0;
    camera.k_inv[1] = 1.0;

    camera.f_scale = 1.0;
    camera.k_scale = 1.0;

    for (int i = 0; i < NUM_CAMERA_PARAMS; i++) {
        camera.constrained[i] = 0;
        camera.constraints[i] = 0.0;
        camera.weights[i] = 0.0;
    }

    camera.fisheye = data.m_fisheye;
    camera.f_cx = data.m_fCx;
    camera.f_cy = data.m_fCy;
    camera.f_rad = data.m_fRad;
    camera.f_angle = data.m_fAngle;
    camera.f_focal = data.m_fFocal;

    if (data.m_known_intrinsics) {
        camera.known_intrinsics = 1;
        memcpy(camera.K_known, data.m_K, 9 * sizeof(double));
        memcpy(camera.k_known, data.m_k, 5 * sizeof(double));
    } else {
        camera.known_intrinsics = 0;
    }
}


std::vector<int> RefineCameraParameters(int num_points, 
                                        v3_t *points, v2_t *projs, 
                                        int *pt_idxs, camera_params_t *camera,
                                        double *error_out, 
                                        bool adjust_focal,
                                        bool remove_outliers,
                                        bool optimize_for_fisheye,
                                        bool estimate_distortion,
                                        double min_proj_error_threshold,
                                        double max_proj_error_threshold)
{
    int num_points_curr = num_points;
    v3_t *points_curr = new v3_t[num_points];
    v2_t *projs_curr = new v2_t[num_points];

    memcpy(points_curr, points, num_points * sizeof(v3_t));
    memcpy(projs_curr, projs, num_points * sizeof(v2_t));

    std::vector<int> inliers;

    for (int i = 0; i < num_points; i++)
        inliers.push_back(i);

    int round = 0;

    adjust_focal = false;
    estimate_distortion = false;

    /* First refine with the focal length fixed */
    camera_refine(num_points_curr, points_curr, projs_curr, camera, 0, 0);    

    while (1) {
        printf("[RefineCameraParameters] Calling with %d points\n", 
            num_points_curr);

        camera_refine(num_points_curr, points_curr, projs_curr, camera, 
            adjust_focal ? 1 : 0, estimate_distortion ? 1 : 0);

        if (!remove_outliers)
            break;

        v3_t *points_next = new v3_t[num_points];
        v2_t *projs_next = new v2_t[num_points];

        int count = 0;
        double error = 0.0;
        std::vector<int> inliers_next;

        double *errors = new double[num_points_curr];

        for (int i = 0; i < num_points_curr; i++) {
            v2_t pr = sfm_project_final(camera, points_curr[i], 1,
                estimate_distortion ? 1 : 0);

            double dx = Vx(pr) - Vx(projs_curr[i]);
            double dy = Vy(pr) - Vy(projs_curr[i]);
            double diff = sqrt(dx * dx + dy * dy);

            errors[i] = diff;

            printf("\nPt': (%f %f), Pt: (%f, %f)", Vx(pr), Vy(pr), Vx(projs_curr[i]),Vy(projs_curr[i]));
            error += diff;
        }

        printf("[RefineCameraParameters] Error: %0.3f\n", 
            error / num_points_curr);

        /* Sort and histogram errors */
        double med = kth_element_copy(num_points_curr, 
            iround(0.95 * num_points_curr),
            errors);

        printf("\nMedian :: %lf", med);

        /* We will tolerate any match with projection error < 8.0 */
        double threshold = 1.2 * NUM_STDDEV * med; /* k * stddev */
        threshold = CLAMP(threshold, min_proj_error_threshold, 
            max_proj_error_threshold);  

        // double threshold = min_proj_error_threshold;

        // double threshold = MAX(8.0, med);

        printf("[RefineCameraParameters] Threshold = %0.3f\n", threshold);
        for (int i = 0; i < num_points_curr; i++) {
            if (errors[i] < threshold) {
                inliers_next.push_back(inliers[i]);

                points_next[count] = points_curr[i];
                projs_next[count] = projs_curr[i];

                count++;
            } else {
                if (pt_idxs != NULL) {
                    printf("[RefineCameraParameters] Removing point [%d] with "
                        "reprojection error %0.3f\n", pt_idxs[i], 
                        errors[i]);
                } else {
                    printf("[RefineCameraParameters] Removing point with "
                        "reprojection error %0.3f\n", errors[i]);
                }
            }
        }

#if 1
        qsort(errors, num_points_curr, sizeof(double), compare_doubles);

        double pr_min = errors[0];
        double pr_max = errors[num_points_curr-1];
        double pr_step = (pr_max - pr_min) / NUM_ERROR_BINS;

        /* Break histogram into 10 bins */
        int idx_count = 0;
        for (int i = 0; i < NUM_ERROR_BINS; i++) {
            double max = pr_min + (i+1) * pr_step;
            int start = idx_count;

            while (idx_count < num_points_curr && errors[idx_count] <= max)
                idx_count++;

            int bin_size = idx_count - start;
            printf("   E[%0.3e--%0.3e]: %d [%0.3f]\n", 
                max - pr_step, max, bin_size, 
                bin_size / (double) num_points_curr);
        }
#endif

        delete [] points_curr;
        delete [] projs_curr;
        delete [] errors;

        points_curr = points_next;
        projs_curr = projs_next;

        if (count == num_points_curr)
            break;  /* We're done */

        num_points_curr = count;

        inliers = inliers_next;

        if (count == 0) /* Out of measurements */
            break;

        round++;

        if (error_out != NULL) {
            *error_out = error;
        }
    }

    printf("[RefineCameraParameters] Exiting after %d rounds "
        "with %d / %d points\n", round + 1, num_points_curr, num_points);

    delete [] points_curr;
    delete [] projs_curr;

    return inliers;
}

void SetFocalConstraint(const localize::ImageData& data, 
                                    camera_params_t *params)
{
    if (data.m_has_init_focal) {

        params->constrained[6] = true;
        params->constraints[6] = data.m_init_focal;
        params->weights[6] = /* m_constrain_focal_weighti */ 0.0001;
    }
}

bool CheckCheirality(v3_t p, double* R, double* t) {	
    double pt[3] = { Vx(p), Vy(p), Vz(p) };
    double cam[3];

    pt[0] -= t[0];
    pt[1] -= t[1];
    pt[2] -= t[2];
    matrix_product(3, 3, 3, 1, (double *) R, pt, cam);

    // EDIT!!!
    if (cam[2] > 0.0)
        return false;
    else
        return true;
}

bool ConvertCamera(int num_points, v3_t *points_solve, v2_t *projs_solve,
                    double* P, double *Kinit, double *Rinit, double *tinit, 
                    double proj_estimation_threshold,
                    double proj_estimation_threshold_weak,
                    std::vector<int> &inliers,
                    std::vector<int> &inliers_weak,
                    std::vector<int> &outliers)
{
    matrix_scale(3, 3, Kinit, 1.0 / Kinit[8], Kinit);

    printf("[FindAndVerifyCamera] Estimated intrinsics:\n");
    matrix_print(3, 3, Kinit);
    printf("[FindAndVerifyCamera] Estimated extrinsics:\n");
    matrix_print(3, 3, Rinit);
     
    printf("[FindAndVerifyCamera] Estimated translation:\n");
    matrix_print(1, 3, tinit);
    
    printf("[FindAndVerifyCamera] Estimated P:\n");
    matrix_print(3, 4, P);
    fflush(stdout);
    
    double C[3], Rinv[9];
    matrix_invert(3,Rinit,Rinv);
    matrix_product(3,3,3,1,Rinv,tinit,C);
    matrix_scale(3,1,C,-1.0f,C);
    printf("[FindAndVerifyCamera] Estimated C:\n");
    matrix_print(3,1,C);


    // Negating the negative entries in K
    int neg = (Kinit[0] < 0.0) + (Kinit[4] < 0.0) + (Kinit[8] < 0.0);
    for(int i=0; i < 3; i++) {
        int idx = i*3 + i;
        if(Kinit[idx] < 0.0) {
            Kinit[i*3] *= -1.0f;
            Kinit[(i+1)*3] *= -1.0f;
            Kinit[(i+2)*3] *= -1.0f;
            Rinit[i*3] *= -1.0f;
            Rinit[i*3+1] *= -1.0f;
            Rinit[i*3+2] *= -1.0f;
        }
    }

    double Rigid[12] = 
    { Rinit[0], Rinit[1], Rinit[2], tinit[0],
    Rinit[3], Rinit[4], Rinit[5], tinit[1],
    Rinit[6], Rinit[7], Rinit[8], tinit[2] };
    
    printf("[FindAndVerifyCamera] Estimated Pdash:\n");
    double tmp[12];
    matrix_product(3,3,3,4,Kinit,Rigid,tmp);
    matrix_print(3, 4, tmp);

    double KR[9];
    matrix_product(3,3,3,3,Kinit,Rinit,KR);
    
    double det = KR[0] * (KR[4]*KR[8] - KR[5]*KR[7]) -
        KR[1] * (KR[3]*KR[8] - KR[5]*KR[7]) + 
        KR[2] * (KR[3]*KR[7] - KR[4]*KR[6]);

    double M3[3] = {KR[6], KR[7], KR[8]};
    matrix_scale(3,1,M3,det,M3);


    /* Check cheirality constraint */
    printf("[FindAndVerifyCamera] Checking consistency...\n");
    int num_behind = 0;
    for (int j = 0; j < num_points; j++) {
        double p[4] = { Vx(points_solve[j]), 
            Vy(points_solve[j]),
            Vz(points_solve[j]), 1.0 };
        double q[3], q2[3];

        matrix_product(3, 4, 4, 1, Rigid, p, q);
        matrix_product331(Kinit, q, q2);
        double pimg[2] = { q2[0] / q2[2], q2[1] / q2[2] };  // Negative in original

        printf("\n%lf %lf , %lf %lf", pimg[0], pimg[1], Vx(projs_solve[j]), Vy(projs_solve[j]));
        double diff = 
            (pimg[0] - Vx(projs_solve[j])) * 
            (pimg[0] - Vx(projs_solve[j])) + 
            (pimg[1] - Vy(projs_solve[j])) * 
            (pimg[1] - Vy(projs_solve[j]));

        diff = sqrt(diff);

        if (diff < proj_estimation_threshold)
            inliers.push_back(j);

        if (diff < proj_estimation_threshold_weak) {
            inliers_weak.push_back(j);
        } else {
            printf("[FindAndVerifyCamera] Removing point [%d] "
                "(reproj. error = %0.3f)\n", j, diff);
            outliers.push_back(j);
        }

        // EDIT!!! Original Cheirality Test
//        if (q[2] > 0.0)
//            num_behind++;  /* Cheirality constraint violated */

        // One that I understand?
//        if(!CheckCheirality(v3_new(p[0], p[1], p[2]), Rinit, tinit)) {
//            num_behind++;
//        };
        
        //Verbatim Hartley and Zisserman pg 149
        double normP3[3] = {p[0] - C[0], p[1] - C[1], p[2] - C[2]};
        double dir = M3[0]*normP3[0] + M3[1]*normP3[1] + M3[2]*normP3[2];
        if(dir > 0.0) {
            num_behind++;
        }

        
    }

    if (num_behind >= 0.9 * num_points) {
        printf("[FindAndVerifyCamera] Error: camera is pointing "
            "away from scene %d/%d points\n", num_behind, num_points);
        return false;
    }

    if(inliers_weak.size() < 0.8*num_points) {
        printf("\nRemoved too many points");
    }

    return true;
}

bool FindAndVerifyCamera(int num_points, v3_t *points_solve, v2_t *projs_solve,
                         int *idxs_solve, double* P,
                         double *K, double *R, double *t, 
                         double proj_estimation_threshold,
                         double proj_estimation_threshold_weak,
                         std::vector<int> &inliers,
                         std::vector<int> &inliers_weak,
                         std::vector<int> &outliers)
{
    /* First, find the projection matrix */
    int r = -1; 
    if (num_points >= 9 && P == NULL) {
        P = new double[12];
        r = find_projection_3x4_ransac(num_points, 
            points_solve, projs_solve, 
            P, 4096, proj_estimation_threshold);
    } else {
        r = num_points;
    }

    if (r == -1) {
        printf("[FindAndVerifyCamera] Couldn't find projection matrix\n");
        return false;
    }

    /* If number of inliers is too low, fail */
    if (r <= MIN_INLIERS_EST_PROJECTION) {
        printf("[FindAndVerifyCamera] Too few inliers to use "
            "projection matrix\n");
        return false;
    }

    double KRinit[9], Kinit[9], Rinit[9], tinit[3];
    memcpy(KRinit + 0, P + 0, 3 * sizeof(double));
    memcpy(KRinit + 3, P + 4, 3 * sizeof(double));
    memcpy(KRinit + 6, P + 8, 3 * sizeof(double));

    dgerqf_driver(3, 3, KRinit, Kinit, Rinit);	    

    /* We want our intrinsics to have a certain form */
    //FixIntrinsics(P, Kinit, Rinit, tinit);
    matrix_scale(3, 3, Kinit, 1.0 / Kinit[8], Kinit);

    printf("[FindAndVerifyCamera] Estimated intrinsics:\n");
    matrix_print(3, 3, Kinit);
    printf("[FindAndVerifyCamera] Estimated extrinsics:\n");
    matrix_print(3, 3, Rinit);
     
    int neg = (Kinit[0] < 0.0) + (Kinit[4] < 0.0) + (Kinit[8] < 0.0);
    if(neg == 1) {
        printf("\nOne negative");
        if(Kinit[0] < 0.0) {
            Kinit[0] = -Kinit[0];
            R[0]*=-1.0;
            R[1]*=-1.0;
            R[2]*=-1.0;
        } else if(Kinit[4] < 0.0) {
            Kinit[4] = -Kinit[4];
            R[3]*=-1.0;
            R[4]*=-1.0;
            R[5]*=-1.0; 
        } else {
            Kinit[8] = -Kinit[8];
            R[6]*=-1.0;
            R[7]*=-1.0;
            R[8]*=-1.0;  
        }
    } else if(neg == 3) {
        printf("\nAll negative");
        matrix_scale(3,3,Kinit,-1,Kinit);  
    } else if(neg == 2) {
        printf("\nTwo negative");
        /* Now deal with case of even parity */
        double fix[9];
        matrix_ident(3, fix);
        double tmp[9];

        if (Kinit[0] < 0.0 && Kinit[4] < 0.0) {
            fix[0] = -1.0;
            fix[4] = -1.0;
        } else if (Kinit[0] < 0.0) {
            fix[0] = -1.0;
            fix[8] = -1.0;
        } else if (Kinit[4] < 0.0) {
            fix[4] = -1.0;
            fix[8] = -1.0;
        }
        matrix_product(3, 3, 3, 3, Kinit, fix, tmp);
        memcpy(Kinit, tmp, sizeof(double) * 3 * 3);
    }
    
    /*
    double Kinv[9], tmp2[12];
    matrix_invert(3, Kinit, Kinv);
    matrix_product(3, 3, 3, 4, Kinv, P, tmp2);
    tinit[0] = tmp2[3];
    tinit[1] = tmp2[7];
    tinit[2] = tmp2[11];
    */
    
    
    printf("[FindAndVerifyCamera] Estimated translation:\n");
    matrix_print(1, 3, tinit);
    fflush(stdout);

    /* Check cheirality constraint */
    printf("[FindAndVerifyCamera] Checking consistency...\n");

    double Rigid[12] = 
    { Rinit[0], Rinit[1], Rinit[2], tinit[0],
    Rinit[3], Rinit[4], Rinit[5], tinit[1],
    Rinit[6], Rinit[7], Rinit[8], tinit[2] };

    int num_behind = 0;
    for (int j = 0; j < num_points; j++) {
        double p[4] = { Vx(points_solve[j]), 
            Vy(points_solve[j]),
            Vz(points_solve[j]), 1.0 };
        double q[3], q2[3];

        matrix_product(3, 4, 4, 1, Rigid, p, q);
        matrix_product331(Kinit, q, q2);
        //double pimg[2] = { q2[0] / q2[2], q2[1] / q2[2] };  // Negative in original
        double pimg[2] = { q2[0] / q2[2], q2[1] / q2[2] };  // Negative in original

        printf("\n%lf %lf , %lf %lf", pimg[0], pimg[1], Vx(projs_solve[j]), Vy(projs_solve[j]));
        double diff = 
            (pimg[0] - Vx(projs_solve[j])) * 
            (pimg[0] - Vx(projs_solve[j])) + 
            (pimg[1] - Vy(projs_solve[j])) * 
            (pimg[1] - Vy(projs_solve[j]));

        diff = sqrt(diff);

        if (diff < proj_estimation_threshold)
            inliers.push_back(j);

        if (diff < proj_estimation_threshold_weak) {
            inliers_weak.push_back(j);
        } else {
            printf("[FindAndVerifyCamera] Removing point [%d] "
                "(reproj. error = %0.3f)\n", idxs_solve[j], diff);
            outliers.push_back(j);
        }

        // EDIT!!! Original Cheirality Test
        if (q[2] > 0.0)
            num_behind++;  /* Cheirality constraint violated */

        // One that I understand?
    //    if(!CheckCheirality(v3_new(p[0], p[1], p[2]), Rinit, tinit)) {
//            num_behind++;
//        };
    }

    if (num_behind >= 0.9 * num_points) {
        printf("[FindAndVerifyCamera] Error: camera is pointing "
            "away from scene\n");
        return false;
    }

    memcpy(K, Kinit, sizeof(double) * 9);
    memcpy(R, Rinit, sizeof(double) * 9);
    memcpy(t, tinit, sizeof(double) * 3);

    // #define COLIN_HACK
#ifdef COLIN_HACK
    matrix_ident(3, R);
    t[0] = t[1] = t[2] = 0.0;
#endif

    return true;
}



/* Register a new image with the existing model - 
 * Function modified from bundler/src/Bundle.cpp */

bool BundleRegisterImage(localize::ImageData& data, vector< v3_t >& pt3, 
        vector<v2_t>& pt2, vector< int >& pt3_idx, vector<int>& pt2_idx, 
        double* P, double* Kinit, double* Rinit, double* tinit) {
    
    clock_t start  = clock();
    /* **** Connect the new camera to any existing points **** */
    int num_keys = (int) data.m_keys_desc.size();
    int num_pts_solve = pt3.size();
    v3_t *points_solve = pt3.data();
    v2_t *projs_solve = pt2.data();
    int *idxs_solve = pt3_idx.data();
    int *keys_solve = pt2_idx.data();


    /* **** Solve for the camera position **** */
    printf("[BundleRegisterImage] Initializing camera...\n");
    FixIntrinsicsRajvi(Kinit,Rinit,tinit);

    camera_params_t *camera_new = new camera_params_t;
    InitializeCameraParams(data, *camera_new);
    ClearCameraConstraints(camera_new);

    /* Set up the new camera */
    memcpy(camera_new->R, Rinit, 9 * sizeof(double));
    matrix_transpose_product(3, 3, 3, 1, Rinit, tinit, camera_new->t);
    matrix_scale(3, 1, camera_new->t, -1.0, camera_new->t);

    camera_new->f = 0.5 * (abs(Kinit[0]) + abs(Kinit[4]));
    if (data.m_has_init_focal) {
        double ratio;
        double init = data.m_init_focal;
        double obs = 0.5 * (abs(Kinit[0]) + abs(Kinit[4]));

        printf("[BundleRegisterImage] "
                "Camera has initial focal length of %0.3f\n", init);

        if (init > obs) ratio = init / obs;
        else            ratio = obs / init;

        if (ratio < 1.4) {
            camera_new->f = init;
            SetFocalConstraint(data, camera_new);
        } else {
            printf("[BundleRegisterImage] "
                    "Estimated focal length of %0.3f "
                    "is too different\n", obs);
            camera_new->f = 0.5 * (abs(Kinit[0]) + abs(Kinit[4]));   
        }
    } else {
        printf("\nNo EXIF Focal");
        camera_new->f = 0.5 * (abs(Kinit[0]) + abs(Kinit[4]));
    }

    /* **** Finally, start the bundle adjustment **** */
    printf("[BundleRegisterImage] Adjusting [%d,%d]...\n",
        (int) pt3.size(), (int) pt3.size());

    int num_points_final = (int) pt3.size();
    v3_t *points_final = pt3.data();;
    v2_t *projs_final = pt2.data();


    std::vector<int> inliers_final;    
    double m_min_proj_error_threshold = 8.0;
    double m_max_proj_error_threshold = 16.0;
    bool m_fixed_focal_length = true;
    inliers_final = RefineCameraParameters(
            num_points_final, 
        points_final, projs_final, 
        NULL, camera_new, 
        NULL, false, true,
        false /*m_optimize_for_fisheye*/,
        false /*m_estimate_distortion*/,
        m_min_proj_error_threshold,
        m_max_proj_error_threshold);

    int num_inliers = (int) inliers_final.size();

    bool success = false;
#define MIN_INLIERS_ADD_IMAGE 12
    //@aditya DANGER!!!
    if (num_inliers < 0.5 * num_points_final) {
        printf("[BundleRegisterImage] "
            "Threw out too many outliers [%d / %d]\n", 
            num_inliers, num_points_final);
    } else if (num_inliers >= MIN_INLIERS_ADD_IMAGE) {
        printf("[BundleRegisterImage] Final camera parameters:\n");
        printf("f: %0.3f\n", camera_new->f);
        printf("R:\n");
        matrix_print(3, 3, camera_new->R);
        printf("t: ");
        matrix_print(1, 3, camera_new->t);

        fflush(stdout);

        data.m_camera.m_adjusted = true;

        memcpy(data.m_camera.m_R, camera_new->R, 9 * sizeof(double));

        data.m_drop_pt[0] = camera_new->t[0];
        data.m_drop_pt[1] = camera_new->t[1];
        data.m_drop_pt[2] = camera_new->t[2];

        data.m_camera.SetPosition(data.m_drop_pt);
        data.m_camera.m_focal = camera_new->f;
        data.m_camera.Finalize();

        /* Connect the keys to the points */
        int num_inliers_final = (int) inliers_final.size();

        for (int i = 0; i < num_keys; i++) {
            data.m_keys_desc[i].m_extra = -1;
        }

        for (int i = 0; i < num_inliers_final; i++) {
            int key_idx = keys_solve[inliers_final[i]];
            int pt_idx = idxs_solve[inliers_final[i]];

            printf("(k,p)[%d] = %d, %d\n", i, key_idx, pt_idx);
            double proj[2];
            data.m_camera.Project(pt3[pt_idx].p, proj);
            printf("pr[%d] = %0.3f, %0.3f == %0.3f, %0.3f\n", 
                i, proj[0], proj[1], 
                data.m_keys_desc[key_idx].m_x, 
                data.m_keys_desc[key_idx].m_y);

            data.m_keys_desc[key_idx].m_extra = pt_idx;
            data.m_visible_points.push_back(pt_idx);
            data.m_visible_keys.push_back(key_idx);
        }

        success = true;
    }

    delete camera_new;

    clock_t end = clock();

    printf("[BundleRegisterImage] Registration took %0.3fs\n",
        (double) (end - start) / (double) CLOCKS_PER_SEC);

    return success;
}


