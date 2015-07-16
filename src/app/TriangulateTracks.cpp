#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <tr1/tuple>
#include <iostream>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <cstring>
#include <queue>
#include <stdlib.h>
#include <algorithm>
#include <set>
#include <math.h>
#include <time.h>

#include "matrix.h"
#include "triangulate.h"
#include "fmatrix.h"
#include "vector.h"
#include "jpeglib.h"

#include "../base/Geometric.h"
#include "../base/Bundle.h"
#include "../base/Reader.h"
#include "../base/argvparser.h"



using namespace std;
using namespace CommandLineProcessing;

#define RAD2DEG(r) ((r) * (180.0 / 3.1416))
#define CLAMP(x,mn,mx) (((x) < mn) ? mn : (((x) > mx) ? mx : (x)))

typedef struct pointStruct { 
    int viewId;
    int siftId;
    double xcord;
    double ycord;
}pointstr;


bool RemoveBadPointsAndCameras(bundle::Bundle* bdl, v3_t point, 
        vector<int>& pt_views) {
    double pos[3] = {point.p[0], point.p[1], point.p[2]};
    int num_views = (int) pt_views.size();
    if (num_views == 0)
        return false;

    double max_angle = 0.0;
    for (int j = 0; j < num_views; j++) {
        int v1 = pt_views[j];

        double r1[3];
        const bundle::CamStruct* camera = bdl->getCamSet(v1);
        double t1[3] = {camera->t[0], camera->t[1], camera->t[2]};
        matrix_diff(3, 1, 3, 1, pos, t1, r1);
        double norm = matrix_norm(3, 1, r1);
        matrix_scale(3, 1, r1, 1.0 / norm, r1);

        for (int k = j+1; k < num_views; k++) {
            int v2 = pt_views[k];

            const bundle::CamStruct* camera2 = bdl->getCamSet(v2);
            double t2[3] = {camera2->t[0], camera2->t[1], camera2->t[2]};
            double r2[3];
            matrix_diff(3, 1, 3, 1, pos, t2, r2);
            double norm = matrix_norm(3, 1, r2);
            matrix_scale(3, 1, r2, 1.0 / norm, r2);

            double dot;
            matrix_product(1, 3, 3, 1, r1, r2, &dot);

            double angle = 
                acos(CLAMP(dot, -1.0 + 1.0e-8, 1.0 - 1.0e-8));

            if (angle > max_angle) {
                max_angle = angle;
            }
        }
    }

    if (RAD2DEG(max_angle) < 1.0) {
        //		printf("[RemoveBadPointsAndCamera] "
        //				"Removing point with angle %0.3f\n", RAD2DEG(max_angle));
        return false;
    }
    return true;
}

void InvertDistortion(int n_in, int n_out, double r0, double r1,
        double *k_in, double *k_out) {
    const int NUM_SAMPLES = 20;

    int num_eqns = NUM_SAMPLES;
    int num_vars = n_out;

    double *A = new double[num_eqns * num_vars];
    double *b = new double[num_eqns];

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double t = r0 + i * (r1 - r0) / (NUM_SAMPLES - 1);

        double p = 1.0;
        double a = 0.0;
        for (int j = 0; j < n_in; j++) {
            a += p * k_in[j];
            p = p * t;
        }   

        double ap = 1.0;
        for (int j = 0; j < n_out; j++) {
            A[i * num_vars + j] = ap; 
            ap = ap * a;
        }   

        b[i] = t;
    }   

    dgelsy_driver(A, b, k_out, num_eqns, num_vars, 1); 

    delete [] A;
    delete [] b;
}



v2_t sfm_project_final(double* R, double* t, double f, double* k, v3_t pt, 
        int explicit_camera_centers, int undistort) {
    double b_cam[3], b_proj[3];
    double b[3] = { Vx(pt), Vy(pt), Vz(pt) };
    v2_t proj;

    /* Project! */
    if (!explicit_camera_centers) {
        matrix_product331(R, b, b_cam);
        b_cam[0] += t[0];
        b_cam[1] += t[1];
        b_cam[2] += t[2];
    } else {
        double b2[3];
        b2[0] = b[0] - t[0];
        b2[1] = b[1] - t[1];
        b2[2] = b[2] - t[2];
        matrix_product331(R, b2, b_cam);
    }

    if (true/*!params->known_intrinsics */) {
        b_proj[0] = b_cam[0] * f;
        b_proj[1] = b_cam[1] * f;
        b_proj[2] = b_cam[2];

        b_proj[0] /= -b_proj[2];
        b_proj[1] /= -b_proj[2];

        if (undistort) {
            double rsq = 
                (b_proj[0] * b_proj[0] + b_proj[1] * b_proj[1]) / 
                (f * f);
            double factor = 1.0 + k[0] * rsq + k[1] * rsq * rsq;

            b_proj[0] *= factor;
            b_proj[1] *= factor;
        }
    } else { 
        /* Apply intrinsics */
        /*	double x_n = -b_cam[0] / b_cam[2];
            double y_n = -b_cam[1] / b_cam[2];

            double *k = params->k_known;
            double rsq = x_n * x_n + y_n * y_n;
            double factor = 1.0 + k[0] * rsq + 
            k[1] * rsq * rsq + k[4] * rsq * rsq * rsq;

            double dx_x = 2 * k[2] * x_n * y_n + k[3] * (rsq + 2 * x_n * x_n);
            double dx_y = k[2] * (rsq + 2 * y_n * y_n) + 2 * k[3] * x_n * y_n;

            double x_d = x_n * factor + dx_x;
            double y_d = y_n * factor + dx_y;

            double *K = params->K_known;
            b_proj[0] = K[0] * x_d + K[1] * y_d + K[2];
            b_proj[1] = K[4] * y_d + K[5];   */      
    }

    Vx(proj) = b_proj[0];
    Vy(proj) = b_proj[1];

    return proj;
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

v2_t UndistortNormalizedPoint(v2_t p, double* kinv)  {
    double r = sqrt(Vx(p) * Vx(p) + Vy(p) * Vy(p));
    if (r == 0.0)
        return p;

    double t = 1.0;
    double a = 0.0;

    for (int i = 0; i < 6 /* POLY_INVERSE_DEGREE */; i++) {
        a += t * kinv[i];
        t = t * r;
    }   

    double factor = a / r;

    return v2_scale(factor, p); 
} 


bool triangulateTrack(bundle::Bundle* bdl, reader::ImageListReader& imList, 
        vector<pointstr> &trackDetails, v3_t& ptOut) { 
    int tsize = trackDetails.size();

    static int tcount = 0;

    double* Rin = new double[tsize*9];
    double* tin = new double[tsize*3];
    double* tlocs = new double[tsize*3];
    double* p_in = new double[tsize*2];
    double* k_in = new double[tsize];
    int* out_inliers = new int[tsize];
    v2_t *pv = new v2_t[tsize];	
    vector<int> pt_views;

    double *x_cordf = new double[tsize];
    double *y_cordf = new double[tsize];

    for(int i = 0; i < trackDetails.size(); i++) { 
        int view = trackDetails[i].viewId;
        pt_views.push_back(view);

        int siftId = trackDetails[i].siftId;

        for(int j=0;j<9;j++) {
            //Rin[i*9+j]= camset[view].R[j];
            Rin[i*9+j]= (bdl->getCamSet(view))->R[j];
        }

        for(int j=0;j<3;j++) { 
            //            tin[i*3+j] = -1*cam_pos[view*3+j];
            tin[i*3+j] = -1*(bdl->getCamPos(view)).el[j];
        }

        for(int j = 0; j < 3; j++) { 
            tlocs[i*3+j] = bdl->getCamLocs(view).el[j];
        }

        double p3[] = {trackDetails[i].xcord, trackDetails[i].ycord, 1.0};

        x_cordf[i] = p3[0];
        y_cordf[i] = p3[1];

        p_in[i*2] = p3[0]; p_in[i*2+1] = p3[1];

        double focal_length = bdl->getFocalLength(view);	
        k_in[i] = focal_length;

        double K[] = {focal_length, 0, 0,
            0, focal_length, 0,
            0, 0 ,1};
        double Kinv[9];

        matrix_invert(3, K, Kinv);
        double p_n[3];
        matrix_product(3, 3, 3, 1, Kinv, p3, p_n);

        pv[i] = v2_new(-p_n[0], -p_n[1]);
        //DANGER!!!
        //
        // Computing K inv now
        double k_inv[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 0.0};

        double k[2] = {bdl->getR1(view), bdl->getR2(view)};

        double k_dist[6] = { 0.0, 1.0, 0.0, k[0], 0.0, k[1] };
        double w_2 = 0.5 * imList.getImageWidth(view);
        double h_2 = 0.5 * imList.getImageHeight(view);

        double max_radius = 
            sqrt(w_2 * w_2 + h_2 * h_2) / focal_length;

        InvertDistortion(6, 6, 0.0, max_radius, 
                k_dist, k_inv);

        pv[i] = UndistortNormalizedPoint(pv[i], k_inv);
    }

    double error;
    v3_t pt2 = v3_new(0, 0, 0);

    /*
       int ret_val = 1; 
       int rounds = 100;
       float threshold = 0.5;

       if(tsize <= 3) threshold = 1;

       else if(tsize <=5) threshold = 0.66;


       ret_val = find_triangulation_3x4_ransac( tsize, pv, Rin, tin, &error, rounds, threshold, &pt2, p_in, k_in, out_inliers );
       if(ret_val == -1) return false; 
       */

    pt2 = triangulate_n(tsize, pv, Rin, tin, &error);


    ptOut = pt2;

    double pt[3] = {pt2.p[0], pt2.p[1], pt2.p[2]};

    double totalErr = 0.0;
    bool status = true;

    for(int j = 0; j < tsize; j++) { 

        double proPt[3] = {x_cordf[j], y_cordf[j], 1.0f};
        double focLen = k_in[j];

        int view = trackDetails[j].viewId;

        double k[2] = {bdl->getR1(view), bdl->getR2(view)};

        //double repErr = geometry::computeReproject(pt, proPt, Rin+j*9, tin+j*3, focLen);

        // Last two argument different in Aditya's code
        v2_t pr = sfm_project_final(Rin+j*9, tlocs+j*3, focLen, k, pt2, 1, 1);

        double dx = Vx(pr) - x_cordf[j];
        double dy = Vy(pr) -  y_cordf[j];
        double repErr = dx * dx + dy * dy;
        totalErr += repErr;
        status = status & CheckCheirality(pt2, Rin+j*9, tlocs+j*3); 
    }

    totalErr = sqrt((totalErr*1.0)/tsize);
    bool goodness = false;
    //    printf("\n%d error %lf, repError %lf, Cheirality %d", tcount, error, totalErr, status);
    tcount++;
    //printf("Total Error: %lf, Cheirality Status :%d", totalErr, status);
    if(status && totalErr < 16.0) { 
        return true;
        //goodness = RemoveBadPointsAndCameras(bdl, pt2, pt_views);
        //printf(", Angular goodness %d", goodness);
    }
    //printf("\n");

    return false;
}

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
    cmd.setIntroductoryDescription("Track formation and triangulation from matches");

    //define error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help",""); 
    cmd.defineOption("bundler_path", "Bundler Path", ArgvParser::OptionRequired);
    cmd.defineOption("mode", "Mode", ArgvParser::NoOptionAttribute);
    cmd.defineOption("tracks_dir", "Path to base directory that stores all track files", 
            ArgvParser::OptionRequired);
    cmd.defineOption("image_idx", "image_index", ArgvParser::OptionRequired); 
    /// finally parse and handle return codes (display help etc...)
    int result = cmd.parse(argc, argv);
    if (result != ArgvParser::NoParserError)
    {
        cout << cmd.parseErrorDescription(result);
        exit(-1);
    }
}



int main(int argc, char* argv[]) { 
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string bundlerPath = cmd.optionValue("bundler_path");
    string trackDir = cmd.optionValue("tracks_dir");
    string imageIdxStr = cmd.optionValue("image_idx");

    string mode = "";
    if(cmd.foundOption("mode")) {
        mode = cmd.optionValue("mode");
    }

    int imageIdx = atoi( imageIdxStr.c_str() );

    char file1[1000], file2[1000], file3[1000];
    if(mode == "merged") {
        sprintf(file1, "%s/merged-tracks.txt", trackDir.c_str(), imageIdx);
        sprintf(file2, "%s/triangulated-tracks-final.txt", 
                trackDir.c_str(), imageIdx);
        sprintf(file3, "%s/dbg_triangulated_tracks-final.txt", 
                trackDir.c_str(), imageIdx);
    } else {
        sprintf(file1, "%s/long-tracks-%d.txt", trackDir.c_str(), imageIdx);
        sprintf(file2, "%s/triangulated-tracks-%d.txt", 
                trackDir.c_str(), imageIdx);
        sprintf(file3, "%s/dbg_triangulated_tracks-%d.txt", 
                trackDir.c_str(), imageIdx);
    }

    FILE* inputFile = fopen(file1, "r");
    if(inputFile == NULL) {
        printf("\nNo track file to process %s", file1);
        return 0;
    }

    FILE* outputFile = fopen(file2, "w");
    if(outputFile == NULL) {
        printf("\nOutput file could not be created");
        return -1;
    }

    FILE* outputFile1 = fopen(file3, "w");
    if(outputFile1 == NULL) {
        printf("\nOutput1 file could not be created");
        return -1;
    }


    bundle::Bundle bdl;
    reader::ImageListReader imList( bundlerPath );
    reader::BundleReader br(bundlerPath, &bdl); 

    bool status = imList.read();
    if(!status) {
        cout << "\nProblem reading image list";
        return -1;
    }

    status = br.read();
    if(!status) {
        cout << "\nProblem reading bundle file";
        return -1;
    }

    int trackLength;
    int initialTrackCount = 0, finalTrackCount = 0;

    while( fscanf(inputFile, "%d", &trackLength)!= EOF) {  
        initialTrackCount++;
        int count = 0; 
        vector< pointstr > track;
        while(count < trackLength) {
            pointStruct pt;
            fscanf(inputFile, "%d", &pt.viewId);
            fscanf(inputFile, "%d", &pt.siftId);
            fscanf(inputFile, "%lf", &pt.xcord);
            fscanf(inputFile, "%lf", &pt.ycord);
            track.push_back(pt);
            count++;
        }

        v3_t point;
        bool status = triangulateTrack(&bdl, imList, track, point);
        if(status) {
            finalTrackCount++;
            fprintf(outputFile, "%d", track.size());
            fprintf(outputFile1, "%d", track.size());
            for(int i=0; i < track.size(); i++) {
                fprintf(outputFile, " %d", track[i].viewId);
                fprintf(outputFile, " %d", track[i].siftId);
                fprintf(outputFile, " %lf", track[i].xcord);
                fprintf(outputFile, " %lf", track[i].ycord);
                
                fprintf(outputFile1, " %d", track[i].viewId);
                fprintf(outputFile1, " %d", track[i].siftId);
            }
            fprintf(outputFile," %lf %lf %lf\n", 
                    point.p[0],  point.p[1], point.p[2]);
            fprintf(outputFile1,"\n");
        }
        //printf("%d...", finalTrackCount);
    }
    fclose(inputFile);
    fclose(outputFile);
    fclose(outputFile1);
    printf("Done writing triangulated tracks for image %d, Initial  %d, Final %d\n",
            imageIdx, initialTrackCount, finalTrackCount);
    return 0;
}

