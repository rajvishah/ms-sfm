/* 
 * CompareModels.cpp
 *
 * Compare two models in terms of points and cameras distances, after
 * performing an alignment between the two camera sets (with a
 * similarity transform)
 *
 * Author: Noah Snavely
 * Copyright (c) 2011 Cornell University
 *
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ext/hash_map>

#include <assert.h>

#include "defines.h"
#include "sfm.h"
#include "matrix.h"
#include "horn.h"
#include "qsort.h"
#include "util.h"

typedef std::pair<int, int> ImageKey;
typedef std::pair<int, int> MatchPair;

typedef struct 
{
    double pos[3];
    double color[3];

    std::vector<ImageKey> views;
} point_t;

namespace __gnu_cxx {    
    template<> struct hash< ImageKey > {
        size_t operator()( const ImageKey& x ) const {
            return x.first * 17 + x.second;
        }
    };
}


void ReadBundleFile(char *bundle_file, bool use_points,
                    std::vector<camera_params_t> &cameras,
                    std::vector<point_t> &points, double &bundle_version)
{
    FILE *f = fopen(bundle_file, "r");
    if (f == NULL) {
	printf("Error opening file %s for reading\n", bundle_file);
	return;
    }

    int num_images, num_points;

    char first_line[256];
    fgets(first_line, 256, f);
    if (first_line[0] == '#') {
        double version;
        sscanf(first_line, "# Bundle file v%lf", &version);

        bundle_version = version;
        printf("[ReadBundleFile] Bundle version: %0.3f\n", version);

        fscanf(f, "%d %d\n", &num_images, &num_points);
    } else if (first_line[0] == 'v') {
        double version;
        sscanf(first_line, "v%lf", &version);
        bundle_version = version;
        printf("[ReadBundleFile] Bundle version: %0.3f\n", version);

        fscanf(f, "%d %d\n", &num_images, &num_points);
    } else {
        bundle_version = 0.1;
        sscanf(first_line, "%d %d\n", &num_images, &num_points);
    }

    printf("[ReadBundleFile] Reading %d images and %d points...\n",
	   num_images, num_points);

    /* Read cameras */
    for (int i = 0; i < num_images; i++) {
	double focal_length, k0, k1;
	double R[9];
	double t[3];
        
        if (bundle_version < 0.2) {
            /* Focal length */
            fscanf(f, "%lf\n", &focal_length);
        } else {
            fscanf(f, "%lf %lf %lf\n", &focal_length, &k0, &k1);
        }

	/* Rotation */
	fscanf(f, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n", 
	       R+0, R+1, R+2, R+3, R+4, R+5, R+6, R+7, R+8);
	/* Translation */
	fscanf(f, "%lf %lf %lf\n", t+0, t+1, t+2);

        // if (focal_length == 0.0)
        //     continue;

        camera_params_t cam;

        cam.f = focal_length;
        memcpy(cam.R, R, sizeof(double) * 9);
        memcpy(cam.t, t, sizeof(double) * 3);

        cameras.push_back(cam);
    }

    if (!use_points) {
        fclose(f);
        return;
    }
    
    /* Read points */
    for (int i = 0; i < num_points; i++) {
	point_t pt;

	/* Position */
	fscanf(f, "%lf %lf %lf\n", 
	       pt.pos + 0, pt.pos + 1, pt.pos + 2);

	/* Color */
	fscanf(f, "%lf %lf %lf\n", 
	       pt.color + 0, pt.color + 1, pt.color + 2);

	int num_visible;
	fscanf(f, "%d", &num_visible);

	for (int j = 0; j < num_visible; j++) {
	    int view, key;
	    fscanf(f, "%d %d", &view, &key);

            double x, y;
            if (bundle_version >= 0.3)
                fscanf(f, "%lf %lf", &x, &y);

            pt.views.push_back(ImageKey(view, key));
	}

        points.push_back(pt);
    }

    fclose(f);
}

std::vector<MatchPair> 
    FindPointCorrespondence(const std::vector<point_t> &points1, 
                            const std::vector<point_t> &points2)
{
    int num_points1 = (int) points1.size();
    int num_points2 = (int) points2.size();

    __gnu_cxx::hash_map<ImageKey, int> view_map;

    for (int i = 0; i < num_points1; i++) {
        int num_views = (int) points1[i].views.size();

        for (int j = 0; j < num_views; j++) {
            view_map[points1[i].views[j]] = i;
        }
    }

    std::vector<MatchPair> pairs;

    for (int i = 0; i < num_points2; i++) {
        int num_views = (int) points2[i].views.size();
        
        for (int j = 0; j < num_views; j++) {
            if (view_map.find(points2[i].views[j]) != view_map.end()) {
                int pt_idx = view_map[points2[i].views[j]];
                pairs.push_back(MatchPair(pt_idx, i));
                // printf("[FindPointCorrespondence] %d <-> %d\n", pt_idx, i);
                break;
            }
        }
    }

    printf("[FindPointCorrespondence] Found %d matches\n", (int) pairs.size());

    return pairs;
}

void AlignModels(const std::vector<camera_params_t> &cameras1,
                 const std::vector<point_t> &points1, 
                 const std::vector<camera_params_t> &cameras2,
                 const std::vector<point_t> &points2, 
                 const std::vector<MatchPair> &pairs, double *T,
                 double ransac_threshold)
{
    int num_matches = (int) pairs.size();

    v3_t *left = NULL;
    v3_t *right = NULL;
    
    if (false && num_matches >= 6) {
        left = new v3_t[num_matches];
        right = new v3_t[num_matches];
    
        for (int i = 0; i < num_matches; i++) {
            int i1 = pairs[i].first;
            int i2 = pairs[i].second;

            const double *p1 = points1[i1].pos;
            const double *p2 = points2[i2].pos;

            left[i]  = v3_new(p2[0], p2[1], p2[2]);
            right[i] = v3_new(p1[0], p1[1], p1[2]);
        }
    } else {
        /* Align using the cameras */
        printf("[AlignModels] Using cameras for alignment\n");

        int num_cameras = (int) cameras1.size();
        num_matches = num_cameras;

        left = new v3_t[num_matches];
        right = new v3_t[num_matches];

        int count = 0;
        for (int i = 0; i < num_cameras; i++) {
            double pos1[3], pos2[3];

            if (cameras1[i].f == 0.0 || cameras2[i].f == 0.0)
                continue;
        
            matrix_transpose_product(3, 3, 3, 1, 
                                     (double *) cameras1[i].R, 
                                     (double *) cameras1[i].t, pos1);
            matrix_transpose_product(3, 3, 3, 1, 
                                     (double *) cameras2[i].R, 
                                     (double *) cameras2[i].t, pos2);

            matrix_scale(3, 1, pos1, -1.0, pos1);
            matrix_scale(3, 1, pos2, -1.0, pos2);

            left[count] = v3_new(pos2[0], pos2[1], pos2[2]);
            right[count] = v3_new(pos1[0], pos1[1], pos1[2]);

            count++;
        }

        num_matches = count;
    }

    /* Run the horn solver */
    int inliers = align_horn_3D_ransac(num_matches, right, left, 
                                       4096, ransac_threshold, 0.0, T);

    printf("[AlignModels] %d / %d inliers\n", inliers, num_matches);
    matrix_print(4, 4, T);
}

void CompareModels(const std::vector<camera_params_t> &cameras1, 
                   const std::vector<point_t> &points1,
                   const std::vector<camera_params_t> &cameras2, 
                   const std::vector<point_t> &points2,
                   const std::vector<MatchPair> &pairs,
                   double *T)
{
    /* Compare camera positions */
    int num_cameras = (int) cameras1.size();
    
    double max_dist = 0.0;
    double sum_dist = 0.0;
    int max_cam = -1;
    
    int good_cameras = 0, good_cameras1 = 0, good_cameras2 = 0;
    double *dists = new double[num_cameras];

    double R[9] = { T[0], T[1], T[2], 
                    T[4], T[5], T[6],
                    T[8], T[9], T[10] };

    double S = sqrt(T[0] * T[0] + T[1] * T[1] + T[2] * T[2]);
    matrix_scale(3, 3, R, 1.0 / S, R);

    FILE *f = fopen("dists.txt", "w");
    FILE *fr = fopen("viewangles.txt", "w");
    for (int i = 0; i < num_cameras; i++) {
        double pos1[3], pos2[4], Tpos2[4];

        if (cameras1[i].f > 0.0)
            good_cameras1++;
        
        if (cameras2[i].f > 0.0)
            good_cameras2++;

        if (cameras1[i].f <= 0.0 || cameras2[i].f <= 0.0)
            continue;
        
        matrix_transpose_product(3, 3, 3, 1, 
                                 (double *) cameras1[i].R, 
                                 (double *) cameras1[i].t, pos1);
        matrix_transpose_product(3, 3, 3, 1, 
                                 (double *) cameras2[i].R, 
                                 (double *) cameras2[i].t, pos2);

        matrix_scale(3, 1, pos1, -1.0, pos1);
        matrix_scale(3, 1, pos2, -1.0, pos2);

        pos2[3] = 1.0;
        
        matrix_product(4, 4, 4, 1, T, pos2, Tpos2);

        double diff[3];
        matrix_diff(3, 1, 3, 1, pos1, Tpos2, diff);
        
        double dist = matrix_norm(3, 1, diff);
        fprintf(f, "%d %0.8f\n", i, dist);

        double *viewdir1 = (double *) cameras1[i].R + 6;
        double *viewdir2 = (double *) cameras2[i].R + 6;

        double Rvd2[3];
        matrix_product(3, 3, 3, 1, R, viewdir2, Rvd2);

        double dot;
        matrix_product(1, 3, 3, 1, viewdir1, Rvd2, &dot);
        double angle = RAD2DEG(acos(CLAMP(dot, -1.0, 1.0)));

        fprintf(fr, "%d %0.8f\n", i, angle);

        if (dist > max_dist) {
            max_dist = dist;
            max_cam = i;
        }
        
        sum_dist += dist;

        dists[good_cameras] = dist;

        good_cameras++;        
    }
    fclose(f);
    fclose(fr);

    double med_dist = median(good_cameras, dists);
    delete [] dists;

    printf("[CompareModels] good cameras (1): %d / %d\n", 
           good_cameras1, num_cameras);
    printf("[CompareModels] good cameras (2): %d / %d\n", 
           good_cameras2, num_cameras);
    printf("[CompareModels] camera dist (avg): %0.4e\n", 
           sum_dist / good_cameras);
    printf("[CompareModels] camera dist (med): %0.4e %0.4f\n", 
           med_dist, med_dist * 1.7588);
    printf("[CompareModels] camera dist (max): %0.4e [cam: %d]\n", 
           max_dist, max_cam);

    /* Compare point positions */
    int num_matches = (int) pairs.size();

    max_dist = 0.0;
    sum_dist = 0.0;
    
    dists = new double[num_matches];
    for (int i = 0; i < num_matches; i++) {
        int i1 = pairs[i].first;
        int i2 = pairs[i].second;

        const double *p1 = points1[i1].pos;
        const double *p2 = points2[i2].pos;

        double p2_4[4] = { p2[0], p2[1], p2[2], 1.0 }, Tp2[4];
        matrix_product(4, 4, 4, 1, T, p2_4, Tp2);

        double diff[3];
        matrix_diff(3, 1, 3, 1, (double *) p1, Tp2, diff);

        double dist = matrix_norm(3, 1, diff);

        if (dist > max_dist)
            max_dist = dist;

        dists[i] = dist;
        
        sum_dist += dist;
    }

    med_dist = median(num_matches, dists);
    delete [] dists;

    printf("[CompareModels] num points (1): %d\n", (int) points1.size());
    printf("[CompareModels] num points (2): %d\n", (int) points2.size());
    printf("[CompareModels] point dist (avg): %0.4e\n", 
           sum_dist / num_matches);
    printf("[CompareModels] point dist (med): %0.4e %0.4f\n", 
           med_dist, med_dist * 1.7588);

    printf("[CompareModels] point dist (max): %0.4e\n", max_dist);
}

void TransformWorld(std::vector<camera_params_t> &cameras, 
                    std::vector<point_t> &points,
                    double *T)
{
    int num_images = (int) cameras.size();
    int num_points = (int) points.size();

    /* Transform the points */
    for (int i = 0; i < num_points; i++) {
	double *pos = points[i].pos;
	double p[4] = { pos[0], pos[1], pos[2], 1.0 }, Tp[4];

	matrix_product(4, 4, 4, 1, T, p, Tp);
	memcpy(pos, Tp, 3 * sizeof(double));
    }

    /* Transform the cameras */
    for (int i = 0; i < num_images; i++) {
	if (cameras[i].f == 0.0)
	    continue;

        double *R = cameras[i].R;
        double *t = cameras[i].t;

	double pose[9], pos[4];	
        
	matrix_transpose(3, 3, R, pose);
        matrix_product(3, 3, 3, 1, pose, t, pos);
        matrix_scale(3, 1, pos, -1.0, pos);
	
	pos[3] = 1.0;
	
	double M3x3[9];
	memcpy(M3x3 + 0, T + 0, 3 * sizeof(double));
	memcpy(M3x3 + 3, T + 4, 3 * sizeof(double));
	memcpy(M3x3 + 6, T + 8, 3 * sizeof(double));
        double scale;
        matrix_product(1, 3, 3, 1, M3x3, M3x3, &scale);
        matrix_scale(3, 3, M3x3, 1.0 / sqrt(scale), M3x3);

	double pose_new[9], pos_new[4];
	matrix_product(3, 3, 3, 3, M3x3, pose, pose_new);
	matrix_product(4, 4, 4, 1, T, pos, pos_new);

	/* Factor out the scaling */
	double mag = matrix_norm(3, 1, pose_new);
	matrix_scale(3, 3, pose_new, 1.0 / mag, pose_new);

	// m_image_data[i].m_camera.SetPose(pose_new);
	// m_image_data[i].m_camera.SetPosition(pos_new);

        matrix_transpose(3, 3, pose_new, R);
        matrix_product(3, 3, 3, 1, R, pos_new, t);
        matrix_scale(3, 1, t, -1.0, t);
    }
}                   

void OutputBundleFile_v3(const std::vector<camera_params_t> &cameras, 
                         const std::vector<point_t> &points,
                         const char *output_file)
{
    /* Output the new bundle.out file */
    FILE *f = fopen(output_file, "w");
    if (f == NULL) {
	printf("[OutputBundleFile] Error opening file %s "
	       "for writing\n", output_file);
	return;
    }

    fprintf(f, "# Bundle file v0.3\n");
    
    int num_images = (int) cameras.size();
    int num_points = (int) points.size();

    fprintf(f, "%d %d\n", num_images, num_points);

    /* Dump cameras */
    for (int i = 0; i < num_images; i++) {
        if (cameras[i].f == 0.0) {
            fprintf(f, "%0.9e %0.9e %0.9e\n", 0.0, 0.0, 0.0);
            fprintf(f, "%0.9e %0.9e %0.9e\n", 0.0, 0.0, 0.0);
            fprintf(f, "%0.9e %0.9e %0.9e\n", 0.0, 0.0, 0.0);
            fprintf(f, "%0.9e %0.9e %0.9e\n", 0.0, 0.0, 0.0);
            fprintf(f, "%0.9e %0.9e %0.9e\n", 0.0, 0.0, 0.0);
        } else {
            const double *R = cameras[i].R;
            const double *t = cameras[i].t;

            fprintf(f, "%0.9e %0.9e %0.9e\n", cameras[i].f, 0.0, 0.0);
            fprintf(f, "%0.9e %0.9e %0.9e\n", R[0], R[1], R[2]);
            fprintf(f, "%0.9e %0.9e %0.9e\n", R[3], R[4], R[5]);
            fprintf(f, "%0.9e %0.9e %0.9e\n", R[6], R[7], R[8]);

            fprintf(f, "%0.9e %0.9e %0.9e\n", t[0], t[1], t[2]);
        }
    }
    
    /* Dump points */
    for (int i = 0; i < num_points; i++) {
        const point_t &p = points[i];
        const double *pos = p.pos;
        const double *color = p.color;

	/* Position */
	fprintf(f, "%0.9e %0.9e %0.9e\n", pos[0], pos[1], pos[2]);

	/* Color */
	fprintf(f, "%d %d %d\n", 
                iround(color[0]), iround(color[1]), iround(color[2]));

        // fprintf(f, "0\n"); /* no points for now */
        int num_visible = (int) p.views.size();
	fprintf(f, "%d", num_visible);
	for (int j = 0; j < num_visible; j++) {
	    int view = p.views[j].first;
            int key = p.views[j].second;

            /* Dummy keypoint locations */
            double x = 0.0;
            double y = 0.0;

	    fprintf(f, " %d %d %0.4f %0.4f", view, key, x, y);
	}

	fprintf(f, "\n");
    }
    
    fclose(f);
}

/* Aligns model 2 to model 1 (but outputs a transformed model 1) */
int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4 && argc != 5 && argc != 6) {
        printf("Usage: %s <bundle1.out> <bundle2.out> "
               "[ransac_threshold (25.0)] [read_points (0)] "
               "[output_file1]\n", argv[0]);
        return 1;
    }
    
    char *bundle1 = argv[1];
    char *bundle2 = argv[2];
    
    double ransac_threshold = 25.0;
    if (argc >= 4) {
        ransac_threshold = atof(argv[3]);
    }

    bool use_points = false;
    if (argc >= 5) {
        use_points = atoi(argv[4]);
    }

    char *output_file = NULL;
    if (argc == 6) {
        output_file = argv[5];
    }

    printf("RANSAC threshold: %0.3f\n", ransac_threshold);

    /* Read bundle files */
    std::vector<camera_params_t> cameras1;
    std::vector<point_t> points1;
    double bundle_version1;
    ReadBundleFile(bundle1, use_points, cameras1, points1, bundle_version1);

    std::vector<camera_params_t> cameras2;
    std::vector<point_t> points2;
    double bundle_version2;
    ReadBundleFile(bundle2, use_points, cameras2, points2, bundle_version2);

    assert(cameras1.size() == cameras2.size());

    /* Establish a point correspondence */
    std::vector<MatchPair> pairs = FindPointCorrespondence(points1, points2);
    
    /* Align the models */
    double T[16];
    AlignModels(cameras1, points1, cameras2, points2, pairs, T, 
                ransac_threshold);
    
    /* Finally, compare the models */
    CompareModels(cameras1, points1, cameras2, points2, pairs, T);

    /* Write the output file if a destination is given */
    if (output_file != NULL) {
        double Tinv[16];
        matrix_invert(4, T, Tinv);
        
        TransformWorld(cameras1, points1, Tinv);
        OutputBundleFile_v3(cameras1, points1, output_file);
    }

    return 0;
}
