#include <stdio.h>
#include <string.h>
#include <cstdlib>
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
#include <math.h>

#include "fmatrix.h"
#include "matrix.h"
#include "vector.h"
#include "jpeglib.h"
#include "triangulate.h"
#include<flann/flann.hpp>
//#include "Geometric.h"

using namespace std;

#define RAD2DEG(r) ((r) * (180.0 / 3.1416))
#define CLAMP(x,mn,mx) (((x) < mn) ? mn : (((x) > mx) ? mx : (x)))

/** 
 *
 * FILE: merge_triangulated_tracks.cpp
 * Authors: Krishna Kumar Singh and Aditya Deshpande
 * Usage: ./a.out <triangulated tracks dir>
 *
 * This code merges the triangulated tracks to get
 * reduced set of consistent tracks.
 *
 **/

struct camstr {
    double t[3];
    double R[9];
    double k_inv[6];
}Camera;

struct camstr* camset;
float* cam_locs;
double* cam_pos;
double* kf;
double* P_orig;
int num_points, num_images;

map < unsigned long long int, vector <unsigned long long int> > my_map ;
map < unsigned long long int, vector <int> > my_track ;
map < unsigned long long int, int > size_vec;
map < unsigned long long int, double> x_cord;
map < unsigned long long int, double> y_cord;

map<unsigned long long int, vector <unsigned long long int> >::iterator iter;
map < unsigned long long int, int > visited;
vector < vector < unsigned long long int> > track;
vector < vector <int> > parent_track;

int g_cnt=0;

void InvertDistortion(int n_in, int n_out, double r0, double r1,
        double *k_in, double *k_out)
{
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

void ReadBundle(char* filePath) {

    string filePathStr( filePath );
    string fileNameStr = filePathStr + "/bundle.out";
    const char* filename = fileNameStr.c_str();
    double version;


    printf("[ReadBundleFile] Reading File %s\n", filename);

    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        printf("[ERROR] opening file %s for reading\n", filename);
        exit(1);
    }

    char first_line[256];
    fgets(first_line, 256, f);

    if (first_line[0] == '#') {
        sscanf(first_line, "# Bundle file v%lf", &version);
        printf("[ReadBundleFile] Bundle version: %0.3f\n", version);
        fscanf(f, "%d %d\n", &num_images, &num_points);
    } else if (first_line[0] == 'v') {
        sscanf(first_line, "v%lf", &version);
        printf("[ReadBundleFile] Bundle version: %0.3f\n", version);
        fscanf(f, "%d %d\n", &num_images, &num_points);
    } else {
        version = 0.1;
        sscanf(first_line, "%d %d\n", &num_images, &num_points);
    }


    printf("[SifterApp::ReadBundleFile] Reading %d images and %d points...\n",
            num_images, num_points);

    /*
    string listFileName = filePathStr + "/list_images.txt";
    string dimsFileName = filePathStr + "/image_dims.txt";
    ifstream file1(listFileName.c_str(), ifstream::in);
    ifstream file2(dimsFileName.c_str(), ifstream::in);
    if( ! file1.is_open() ) {
        printf("[ERROR] cannot open images_list file\n");
        exit(1);
    }
    if( ! file2.is_open() ) {
        printf("[ERROR] cannot open image_dims file\n");
        exit(1);
    }

    int width_arr[num_images], height_arr[num_images];
    for(int i=0; i < num_images; i++) {
        file2 >> width_arr[i] >> height_arr[i];
    }
    file2.close();
    file1.close();
    */

    camset = new struct camstr[num_images];
    cam_locs = new float[num_images * 3];
    cam_pos = new double[num_images * 3];	
    kf = new double[num_images*3];
    P_orig = new double[num_images*12];

    printf("[ReadBundleFile] Reading %d camera parameters\n", num_images);
    for (int i = 0; i < num_images; i++) {
        double focal_length;
        double R[9];
        double t[3];
        double k[2] = { 0.0, 0.0 };

        if (version >= 0.4) {
            char name[2];
            int w, h;
            fscanf(f, "%s %d %d\n", name, &w, &h);
        }

        /* Focal length */
        if (version > 0.1) {
            fscanf(f, "%lf %lf %lf\n", &focal_length, k+0, k+1);
        } else {
            fscanf(f, "%lf\n", &focal_length);
        }
        kf[i*3+0] = focal_length;
        kf[i*3+1] = k[0];
        kf[i*3+2] = k[1];

        /* Rotation */
        fscanf(f, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n",
                &camset[i].R[0], &camset[i].R[1], &camset[i].R[2], &camset[i].R[3], &camset[i].R[4], &camset[i].R[5], &camset[i].R[6], &camset[i].R[7], &camset[i].R[8]);



        /* Translation */
        fscanf(f, "%lf %lf %lf\n", &t[0],&t[1],&t[2]);
        t[0] = -1*t[0];
        t[1] = -1*t[1];
        t[2] = -1*t[2];

        cam_pos[i*3] = t[0];	
        cam_pos[i*3+1] = t[1];	
        cam_pos[i*3+2] = t[2];


        /*
        double R1[9];
        matrix_invert(3,camset[i].R,R1);
        matrix_product(3,3,3,1,R1,t,camset[i].t);

        camset[i].k_inv[0] = camset[i].k_inv[2] = camset[i].k_inv[3] = 0.0; 
        camset[i].k_inv[4] = camset[i].k_inv[5] = 0.0; 
        camset[i].k_inv[1] = 1.0; 

        double k_dist[6] = { 0.0, 1.0, 0.0, k[0], 0.0, k[1] };
        double w_2 = 0.5 * width_arr[i];
        double h_2 = 0.5 * height_arr[i];

        double max_radius = 
            sqrt(w_2 * w_2 + h_2 * h_2) / focal_length;

        InvertDistortion(6, 6, 0.0, max_radius, 
                k_dist, camset[i].k_inv);


        cam_locs[i*3] = (float)camset[i].t[0];
        cam_locs[i*3+1] = (float)camset[i].t[1];
        cam_locs[i*3+2] = (float)camset[i].t[2];

        double K[] = {focal_length,0,0,
            0, focal_length, 0,
            0, 0,   1};

        double RT[12];

        for(int i1=0;i1<3;i1++) {
            for(int j1=0;j1<3;j1++)
                RT[i1*4+j1] = camset[i].R[i1*3+j1];

            RT[i1*4+3] = -t[i1];
        }

        matrix_product(3,3,3,4,K,RT,P_orig+12*i);

        for(int i1=8;i1<12;i1++) {
            P_orig[12*i+i1] = -P_orig[12*i+i1];
        }

        */
    }
    double pt1, pt2, pt3;
    int col_r, col_g, col_b;
    int num = 0;

    for (int i = 0; i < num_points; i++) {

        fscanf(f, "%lf %lf %lf", &pt1, &pt2, &pt3);
        fscanf(f, "%d %d %d", &col_r, &col_g, &col_b);
        fscanf(f, "%d", &num);

        unsigned long long int ar[num];
        g_cnt++;

        for(int j=0;j<num;j++) {

            ar[j]=0;
            long long int im_id;
            long long int sift_id;
            fscanf(f,"%lld",&im_id);
            fscanf(f,"%lld",&sift_id);

            // View ID and Sift ID are packed to a single long int
            ar[j]|= im_id <<32;
            ar[j]|= sift_id;

            double temp;

            fscanf(f,"%lf",&temp);
            x_cord[ar[j]]=temp;

            fscanf(f,"%lf",&temp);
            y_cord[ar[j]]=temp;
        }

        // O(N*N) loop to create adjacency list
        for(int j=0;j<num;j++) {

            vector< unsigned long long int> temp;
            int size;

            if( my_map.find(ar[j]) != my_map.end()) {
                temp = my_map[ar[j]];
                size = size_vec[ar[j]];
                int cnt=size;
                for (int k=0;k<num;k++) {
                    if(cnt >= temp.size())
                    {
                        temp.resize(2*temp.size());
                    }
                    temp[cnt]=ar[k];
                    cnt++;		
                }
                my_map[ar[j]]=temp;
                size_vec[ar[j]]=cnt;
                my_track[ar[j]].push_back(g_cnt);
            }
            else {
                if(num<100) {
                    temp.resize(100);
                }
                else {
                    temp.resize(num);
                }
                for (int k=0;k<num;k++)
                {
                    temp[k]=ar[k];
                }
                size_vec[ar[j]]= num;
                my_map[ar[j]]= temp;
                vector <int> track_rec;
                track_rec.push_back(g_cnt);
                my_track[ar[j]]= track_rec;
            }

        }

    }
    fclose(f);
}

int main(int argc, char* argv[]) {

    if(argc!=4) {
        printf("[USAGE] ./merge_tracks <bundleDirPath> <triangulationDirPath> <outputPath>\n");
        return 1;
    }

    char results_dir[1000], bundler_dir[1000], output_dir[1000];
    strcpy(results_dir, argv[2]);
    strcpy(bundler_dir, argv[1]);
    strcpy(output_dir, argv[3]);

    char bundleFile[1000];
    sprintf(bundleFile, "%s/bundle.out", bundler_dir);

    printf("[DEBUG] Calling ReadBundle\n");	
    ReadBundle(bundler_dir);

    printf("[DEBUG] Results Directory : %s\n", results_dir);

    char command[1000];
    //    sprintf(command,"ls %s/triangulated_points_image*.out", results_dir);
    sprintf(command,"ls %s/triangulated-tracks*.txt", results_dir);
    FILE *fSys = popen(command, "r");

    char path[1000];

    int num;
    double rError;

    printf("\nPath for searching tracks %s", command);
    fflush(stdout);

    while(fgets(path, sizeof(path)-1, fSys) != NULL) {
        path[strlen(path)-1] = '\0';
        printf("[DEBUG] Reading Output File: %s\n", path);	
        FILE *f = fopen(path, "r");
        char line[1000];
        char waste[1000];
        char newLineChar;
        int lineCtr = 0;
        //while(fscanf(f, "%lf %d", &rError, &num)!=EOF) { 
        while(fscanf(f, "%d",&num)!=EOF) {
            /*            if(num < 3) {
                          for(int i=0;i<num;i++) {
                          long long int im_id;
                          long long int sift_id;
                          fscanf(f,"%lld",&im_id);
                          fscanf(f,"%lld",&sift_id);

                          double temp;

                          fscanf(f,"%lf",&temp);
                          fscanf(f,"%lf",&temp);

                          }
                          continue;
                          }
                          */
            lineCtr++;
            if(lineCtr%1000 == 0) {
                printf("\nReading file %s, line %d", path, lineCtr);
            }
            /* uncomment for reprojection error part
               if(rError > 4.0)  { 
               fscanf(f, "%[^\n]", waste);
               fscanf(f, "%c", &newLineChar);
               continue;
               } */	
            g_cnt++;
            unsigned long long int ar[num];
            if(ar == NULL) {
                printf("\nCould not allocate memory");
            }

            for(int i=0;i<num;i++) {
                ar[i]=0;
                long long int im_id;
                long long int sift_id;
                fscanf(f,"%lld",&im_id);
                fscanf(f,"%lld",&sift_id);

                // View ID and Sift ID are packed to a single long int
                ar[i]|= im_id << 32;
                ar[i]|= sift_id;

                double temp;

                fscanf(f,"%lf",&temp);
                x_cord[ar[i]]=temp;

                fscanf(f,"%lf",&temp);
                y_cord[ar[i]]=temp;

            }
            double x, y, z;
            fscanf(f,"%lf %lf %lf",&x,&y,&z);

            // O(N*N) loop to create adjacency list
            for(int i=0;i<num;i++) {

                vector< unsigned long long int> temp;
                int size;

                if( my_map.find(ar[i]) != my_map.end()) {
                    temp = my_map[ar[i]];
                    size = size_vec[ar[i]];
                    int cnt=size;
                    for (int j=0;j<num;j++) {
                        if(cnt >= temp.size())
                        {
                            temp.resize(2*temp.size());
                        }
                        temp[cnt]=ar[j];
                        cnt++;		
                    }
                    my_map[ar[i]]=temp;
                    size_vec[ar[i]]=cnt;
                    my_track[ar[i]].push_back(g_cnt);
                }
                else {
                    if(num<100) {
                        temp.resize(100);
                    }
                    else {
                        temp.resize(num);
                    }
                    for (int j=0;j<num;j++)
                    {
                        temp[j]=ar[j];
                    }
                    size_vec[ar[i]]= num;
                    my_map[ar[i]]= temp;
                    vector <int> track_rec;
                    track_rec.push_back(g_cnt);
                    my_track[ar[i]]= track_rec;
                }

            }

        }
        fclose(f);
    }
    printf("[DEBUG] g_cnt %d\n", g_cnt);


    parent_track.resize(100000000);

    track.resize(100000000);

    int track_cnt=0;

    printf("[DEBUG] Initializaing visited vector\n");
    // Initialize the visited vector and resize my_map
    for(iter=my_map.begin();iter!=my_map.end();++iter) {

        vector< unsigned long long int> temp;
        temp = my_map[iter->first];
        temp.resize(size_vec[iter->first]);
        my_map[iter->first]=temp;
        visited[iter->first]=0;
    }

    printf("[DEBUG] Finding connected components\n");
    for(iter=my_map.begin();iter!=my_map.end();++iter) {

        if(visited[iter->first]==0) {

            queue<unsigned long long int> my_queue;
            vector <unsigned long long int> temp_track;
            vector <int> remove_views;
            temp_track.resize(100);
            int cnt=0;
            my_queue.push(iter->first);
            visited[iter->first]=1;
            int parent_cnt=0;

            while(!my_queue.empty()) {

                unsigned long long int temp = my_queue.front();
                if(cnt>= temp_track.size()) {
                    temp_track.resize(temp_track.size()*2);
                }
                int flag=0;

                vector<int>::iterator itr = 
                    find( remove_views.begin(), remove_views.end(), (int)(0|(temp>>32)));
                if(itr != remove_views.end()) {
                    flag = 1;
                } else { 
                    for(int j=0; j<cnt;j++) {
                        unsigned long long int tempu = temp_track[j];
                        if((0|(temp >> 32)) == (0|(tempu >> 32))) {
                            temp_track.erase(temp_track.begin()+j);
                            cnt--;
                            flag=1;
                            remove_views.push_back((int)(0|(temp >> 32)));
                            break;
                        }
                    }
                }

                if(flag==0) {
                    temp_track[cnt]=temp;
                    if(parent_cnt==0) {
                        parent_track[track_cnt]=my_track[temp];
                        parent_cnt = my_track[temp].size();
                    } else {
                        if(my_track[temp].size()>1)
                        {
                            for(int l=0;l<my_track[temp].size();l++)
                            {
                                parent_track[track_cnt].push_back(my_track[temp][l]);
                            }
                        }
                    }
                    cnt++;
                }	
                my_queue.pop();
                vector< unsigned long long int> temp_vec;
                temp_vec = my_map[temp];
                vector<unsigned long long int>::iterator iter1;
                for(iter1=temp_vec.begin();iter1!=temp_vec.end();++iter1) {
                    if(visited[*iter1]==0) {
                        my_queue.push(*iter1);
                        visited[*iter1]=1;
                    }
                }
            }

            sort(parent_track[track_cnt].begin(),  parent_track[track_cnt].end());

            parent_track[track_cnt].erase( unique(  
                        parent_track[track_cnt].begin(),  
                        parent_track[track_cnt].end() ),  
                    parent_track[track_cnt].end() );

            temp_track.resize(cnt);
            track[track_cnt]= temp_track;
            track_cnt++;
        }

    }
    printf("[DEBUG] Done finding connected components, tracks found %d\n", track_cnt);

    track.resize(track_cnt);
    parent_track.resize(track_cnt);
    vector< vector<unsigned long long int> >::iterator iter2;

    char tracksPath[1000];
    sprintf(tracksPath,"%s/merged-tracks.txt",output_dir);
    printf("\nWriting all results to file %s", tracksPath);
    fflush(stdout);

    FILE *ftrack = fopen(tracksPath,"w");
    for(iter2=track.begin(); iter2!=track.end(); ++iter2) {
        vector <unsigned long long int> my_temp;
        my_temp = *iter2;
        int tsize= my_temp.size();
        if(tsize>1) {
            vector<unsigned long long int>::iterator iter1;
            double *x_cordf = new double[tsize];
            double *y_cordf = new double[tsize];
            int i = 0;
            for(iter1=my_temp.begin();iter1!=my_temp.end();++iter1) {
                unsigned long long int tempu = *iter1;
                unsigned long long int view = (0|(tempu >> 32));
                unsigned long long int one = 1;
                unsigned long long int siftid = (tempu & ((one<<32)-1));
                x_cordf[i] = x_cord[tempu];
                y_cordf[i] = y_cord[tempu];
                i++;}
            vector< int > pt_views;
            for(iter1=my_temp.begin();iter1!=my_temp.end();++iter1) {
                unsigned long long int tempu = *iter1;
                pt_views.push_back((int)(0|(tempu >> 32)));}
            if(tsize > 1) { 
                int j = 0;
                fprintf(ftrack, "%d ", tsize);
                for(iter1=my_temp.begin();iter1!=my_temp.end();++iter1) {
                    unsigned long long int tempu = *iter1;
                    unsigned long long int view = (0|(tempu >> 32));
                    unsigned long long int one = 1;
                    unsigned long long int siftid = (tempu & ((one<<32)-1));
                    fprintf(ftrack, "%d %d %lf %lf ", view, siftid, x_cordf[j], y_cordf[j]);
                    j++;
                }
                fprintf(ftrack,"\n");} 
        }
    }
    return 0;
}
