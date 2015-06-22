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
            error += diff;
        }

        printf("[RefineCameraParameters] Error: %0.3f\n", 
            error / num_points_curr);

        /* Sort and histogram errors */
        double med = kth_element_copy(num_points_curr, 
            iround(0.95 * num_points_curr),
            errors);

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

bool FindAndVerifyCamera(int num_points, v3_t *points_solve, v2_t *projs_solve,
                         int *idxs_solve,
                         double *K, double *R, double *t, 
                         double proj_estimation_threshold,
                         double proj_estimation_threshold_weak,
                         std::vector<int> &inliers,
                         std::vector<int> &inliers_weak,
                         std::vector<int> &outliers)
{
    /* First, find the projection matrix */
    double P[12];
    int r = -1;

    if (num_points >= 9) {
        r = find_projection_3x4_ransac(num_points, 
            points_solve, projs_solve, 
            P, /* 2048 */ 4096 /* 100000 */, 
            proj_estimation_threshold);
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
    FixIntrinsics(P, Kinit, Rinit, tinit);
    matrix_scale(3, 3, Kinit, 1.0 / Kinit[8], Kinit);

    printf("[FindAndVerifyCamera] Estimated intrinsics:\n");
    matrix_print(3, 3, Kinit);
    printf("[FindAndVerifyCamera] Estimated extrinsics:\n");
    matrix_print(3, 3, Rinit);
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

        double pimg[2] = { -q2[0] / q2[2], -q2[1] / q2[2] };
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

        // EDIT!!!
        if (q[2] > 0.0)
            num_behind++;  /* Cheirality constraint violated */
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
        vector<v2_t>& pt2, vector< int >& pt3_idx, vector<int>& pt2_idx) {
    
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

    camera_params_t *camera_new = new camera_params_t;
//    InitializeCameraParams(data, *camera_new);
//    ClearCameraConstraints(camera_new);

    double Kinit[9], Rinit[9], tinit[3];
    std::vector<int> inliers, inliers_weak, outliers;
    bool success = 
        FindAndVerifyCamera(num_pts_solve, points_solve, projs_solve,
        idxs_solve, Kinit, Rinit, tinit, 
        /*m_projection_estimation_threshold*/ 4.0, 
        2.0 * 4.0 /* m_projection_estimation_threshold*/,
        inliers, inliers_weak, outliers);


    ClearCameraConstraints(camera_new);

    if (success) {
        /* Set up the new camera */
        memcpy(camera_new->R, Rinit, 9 * sizeof(double));

        matrix_transpose_product(3, 3, 3, 1, Rinit, tinit, camera_new->t);
        matrix_scale(3, 1, camera_new->t, -1.0, camera_new->t);


        double m_init_focal_length = 532.0; 

        // camera_new->f = 0.5 * (Kinit[0] + Kinit[4]);
        if (0 /*m_fixed_focal_length*/) {
            camera_new->f = m_init_focal_length;
        } else {
            if (1 /*m_use_focal_estimate*/ ) {
                if (data.m_has_init_focal) {
                    double ratio;
                    double init = data.m_init_focal;
                    double obs = 0.5 * (Kinit[0] + Kinit[4]);

                    printf("[BundleRegisterImage] "
                        "Camera has initial focal length of %0.3f\n", init);

                    if (init > obs) ratio = init / obs;
                    else            ratio = obs / init;

                    if (ratio < 1.4 || false /*m_trust_focal_estimate*/) {
                        camera_new->f = init;
                        if (1.0 /* m_constrain_focal*/)
                            SetFocalConstraint(data, camera_new);
                    } else {
                        printf("[BundleRegisterImage] "
                            "Estimated focal length of %0.3f "
                            "is too different\n", obs);

                        camera_new->f = 0.5 * (Kinit[0] + Kinit[4]);   
                    }
                } else {
                    camera_new->f = 0.5 * (Kinit[0] + Kinit[4]);
                }
            } else if (data.m_has_init_focal) {
                camera_new->f = data.m_init_focal;
            } else {
                camera_new->f = m_init_focal_length; // cameras[parent_idx].f;
            }
        }
    } else {
        delete camera_new;
        return false;
    }

    /* **** Finally, start the bundle adjustment **** */
    printf("[BundleRegisterImage] Adjusting [%d,%d]...\n",
        (int) inliers.size(), (int) inliers_weak.size());

    int num_points_final = (int) inliers_weak.size();
    v3_t *points_final = new v3_t[num_points_final];
    v2_t *projs_final = new v2_t[num_points_final];

    for (int i = 0; i < num_points_final; i++) {
        points_final[i] = points_solve[inliers_weak[i]];
        projs_final[i] = projs_solve[inliers_weak[i]];
    }

    std::vector<int> inliers_final;

    
    double m_min_proj_error_threshold = 8.0;
    double m_max_proj_error_threshold = 16.0;
    bool m_fixed_focal_length = false;

#if 1
    inliers_final = RefineCameraParameters(
            num_points_final, 
        points_final, projs_final, 
        NULL, camera_new, 
        NULL, !m_fixed_focal_length, true,
        0 /*m_optimize_for_fisheye*/,
        1 /*m_estimate_distortion*/,
        m_min_proj_error_threshold,
        m_max_proj_error_threshold);

#else
    inliers = 
        RefineCameraAndPoints(data, num_points_final,
        points_final, projs_final, idxs_final, 
        cameras, added_order, pt_views, &camera_new, 
        true);
#endif

    int num_inliers = (int) inliers_final.size();

    success = false;
#define MIN_INLIERS_ADD_IMAGE 16
    //@aditya DANGER!!!
    /* if (num_inliers < 0.5 * num_points_final) {
        printf("[BundleRegisterImage] "
            "Threw out too many outliers [%d / %d]\n", 
            num_inliers, num_points_final);
    } else*/ if (num_inliers >= MIN_INLIERS_ADD_IMAGE) {
        printf("[BundleRegisterImage] Final camera parameters:\n");
        printf("f: %0.3f\n", camera_new->f);
        printf("R:\n");
        matrix_print(3, 3, camera_new->R);
        printf("t: ");
        matrix_print(1, 3, camera_new->t);

        fflush(stdout);

#if 0
        data.LoadImage();

        for (int i = 0; i < num_points_final; i++) {
            img_draw_pt(data.m_img, 
                iround(Vx(projs_final[i]) + 0.5 * data.GetWidth()),
                iround(Vy(projs_final[i]) + 0.5 * data.GetHeight()), 
                6, 0xff, 0x0, 0x0);
        }

        img_write_bmp_file(data.m_img, "projs.bmp");
#endif

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
            int key_idx = keys_solve[inliers_weak[inliers_final[i]]];
            int pt_idx = idxs_solve[inliers_weak[inliers_final[i]]];

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

    delete [] points_final;
    delete [] projs_final;

    clock_t end = clock();

    printf("[BundleRegisterImage] Registration took %0.3fs\n",
        (double) (end - start) / (double) CLOCKS_PER_SEC);

    return success;
}


