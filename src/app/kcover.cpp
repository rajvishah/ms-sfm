#include <vector>
#include<map>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <stdio.h>

using namespace std;

typedef struct camstr{
    double t[3];
    double R[9];
    double krd_inv[6]; // related to undistort
}camstr;

class kcover {
    int numPts;
    int numImgs;
    int k;
    vector<bool> validIds;
    vector<int> counts;
    vector<int> range;
    multimap<int,int> trackRecord;
    vector<vector<int> > tracks;
    vector<vector<int> > sifts;
    vector<vector<double> > xs;
    vector<vector<double > > ys;
    vector<vector< double > > trackCoords;
    vector<vector< int > >  trackColors;
    vector<int> coverPoints;
    char* inputFileName;
    char* outputFileName;
    public:
        void ReadBundle(char* filename);
        kcover(char* filename1, char* filename2, int k);
        bool ComputeCoverFast();
        void WriteCover();

};

void kcover::ReadBundle( char* filename ) {
	double version;
	printf("[ReadBundleFile] Reading File %s\n", filename);

	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		printf("[ERROR] opening file %s for reading\n", filename);
		//exit(1);
	}

    int num_points, num_images;
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

	camstr* camset = new struct camstr[num_images];
	float* cam_locs = new float[num_images * 3];
	double* cam_pos = new double[num_images * 3];	
	double* kf = new double[num_images*3];
	double* P_orig = new double[num_images*12];

    printf("[ReadBundleFile] Reading %d camera parameters\n", num_images);
	for (int i = 0; i < num_images; i++) {
        range.push_back(i);
		double focal_length;
		double R[9];
		double t[3];
		double k[2] = { 0.0, 0.0 };

		if (version >= 0.4) {
			char name[2];
			int w, h;
			fscanf(f, "%s %d %d\n", name, &w, &h);
		}

		if (version > 0.1) {
			fscanf(f, "%lf %lf %lf\n", &focal_length, k+0, k+1);
		} else {
			fscanf(f, "%lf\n", &focal_length);
		}

        if(focal_length == 0.0f && k[0] == 0.0f && k[1] == 0.0f) {
            validIds.push_back(false);
        } else {
            validIds.push_back(true);
        }
		kf[i*3+0] = focal_length;
		kf[i*3+1] = k[0];
		kf[i*3+2] = k[1];

		fscanf(f, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n",
				&camset[i].R[0], &camset[i].R[1], &camset[i].R[2], &camset[i].R[3], &camset[i].R[4], &camset[i].R[5], &camset[i].R[6], &camset[i].R[7], &camset[i].R[8]);

		fscanf(f, "%lf %lf %lf\n", &t[0],&t[1],&t[2]);
		t[0] = -1*t[0];
		t[1] = -1*t[1];
		t[2] = -1*t[2];

		cam_pos[i*3] = t[0];	
		cam_pos[i*3+1] = t[1];	
		cam_pos[i*3+2] = t[2];


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

		for(int i1=8;i1<12;i1++) {
			P_orig[12*i+i1] = -P_orig[12*i+i1];
		}
	}
    printf("[ReadBundleFile] Reading %d point parameters\n", num_points);
	double pt1, pt2, pt3;
	int col_r, col_g, col_b;
	int num = 0;

    tracks.resize(num_points);
    sifts.resize(num_points);
    xs.resize(num_points);
    ys.resize(num_points);
    trackColors.resize(num_points);
    trackCoords.resize(num_points);
	for (int i = 0; i < num_points; i++) {
        double p[3];
		fscanf(f, "%lf %lf %lf", &p[0], &p[1], &p[2]);
        
		fscanf(f, "%d %d %d", &col_r, &col_g, &col_b);
		fscanf(f, "%d", &num);

        trackCoords[i].push_back(p[0]);
        trackCoords[i].push_back(p[1]);
        trackCoords[i].push_back(p[2]);

        trackColors[i].push_back(col_r);
        trackColors[i].push_back(col_g);
        trackColors[i].push_back(col_b);
        vector<int>& trackVec = tracks[i]; 
        for(int j=0;j<num;j++) {
            int view;
            int buf;
            double x, y;
            fscanf(f,"%d",&view);
            fscanf(f,"%d",&buf);
            fscanf(f,"%lf",&x);
            fscanf(f,"%lf",&y);

            trackVec.push_back(view);
            sifts[i].push_back(buf); 
            xs[i].push_back(x); 
            ys[i].push_back(y);
        }
        trackRecord.insert(make_pair(num,i));
    }
    numPts = num_points;
    numImgs = num_images;
	fclose(f);
}

bool kcover::ComputeCoverFast() {
    ReadBundle(inputFileName);
    counts.resize(numPts,0);
    multimap<int,int > ::reverse_iterator mItr;
    mItr = trackRecord.rbegin();
    while(mItr != trackRecord.rend()) {
        int currPointId = (*mItr).second;
        vector<int> currRange = tracks[currPointId];
        bool addThisTrack = false;
//        if(currRange.size() < 3) break;
        for(int i=0; i < currRange.size(); i++) {
            int view = currRange[i];
            if(validIds[view] == false) {
                counts[view] = k;
            }
            if(counts[view] < k) {
                addThisTrack = true;
                counts[view]++;
            } 
        }
        if(addThisTrack) {
            coverPoints.push_back(currPointId);
        }
        mItr++;
    }
}


kcover::kcover(char* filename1, char* filename2, int len) {
    inputFileName = filename1;
    outputFileName = filename2;
    k = len; 
} 


void kcover::WriteCover() {
	double version;
	printf("[ReadBundleFile] Reading File %s\n", inputFileName);

	FILE *f = fopen(inputFileName, "r");
	FILE *fw = fopen(outputFileName, "w");
    
    if (fw == NULL ) {
		printf("[ERROR] opening file %s for writing\n", outputFileName);
		return;
    }
    
    if (f == NULL || fw == NULL) {
		printf("[ERROR] opening file %s for reading\n", inputFileName);
		return;
	}

    
	char first_line[256];
	fgets(first_line, 256, f);

    int num_points, num_images;
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
    fprintf(fw,first_line);
	printf("[SifterApp::ReadBundleFile] Reading %d images and %d points...\n",
			num_images, num_points);

    fprintf(fw,"%d %d\n", num_images, coverPoints.size());


    camstr* camset = new struct camstr[num_images];
    float* cam_locs = new float[num_images * 3];
    double* cam_pos = new double[num_images * 3];	
    double* kf = new double[num_images*3];
    double* P_orig = new double[num_images*12];
	for (int i = 0; i < num_images; i++) {
        range.push_back(i);
        
		double focal_length;
		double R[9];
		double t[3];
		double k[2] = { 0.0, 0.0 };

		if (version >= 0.4) {
			char name[2];
			int w, h;
			fscanf(f, "%s %d %d\n", name, &w, &h);
			fprintf(fw, "%s %d %d\n", name, w, h);
		}

		if (version > 0.1) {
			fscanf(f, "%lf %lf %lf\n", &focal_length, k+0, k+1);
            if(!validIds[i]) {
                fprintf(fw,"0 0 0\n"); 
            } else {
    			fprintf(fw, "%lf %lf %lf\n", focal_length, *(k+0), *(k+1));
            }



		} else {
			fscanf(f, "%lf\n", &focal_length);
			fprintf(fw, "%lf\n", focal_length);
		}

		fscanf(f, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n",
				&camset[i].R[0], &camset[i].R[1], &camset[i].R[2], &camset[i].R[3], &camset[i].R[4], &camset[i].R[5], &camset[i].R[6], &camset[i].R[7], &camset[i].R[8]);

        if(!validIds[i]) {
            fprintf(fw,"0 0 0\n0 0 0\n0 0 0\n"); 
        } else {
		fprintf(fw, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n",
				camset[i].R[0], camset[i].R[1], camset[i].R[2], camset[i].R[3], camset[i].R[4], camset[i].R[5], camset[i].R[6], camset[i].R[7], camset[i].R[8]);
        }

		fscanf(f, "%lf %lf %lf\n", &t[0],&t[1],&t[2]);
        if(!validIds[i]) {
            fprintf(fw,"0 0 0\n"); 
        } else {
            fprintf(fw, "%lf %lf %lf\n", t[0],t[1],t[2]);
        }
	}

    printf("[ReadBundleFile] Reading %d point parameters\n", num_points);
	for (int i = 0; i < coverPoints.size(); i++) {
        int point = coverPoints[i];
        vector<int> color = trackColors[point];
        vector<double> cords = trackCoords[point];
        vector<int>& track = tracks[point];

		fprintf(fw, "%lf %lf %lf\n", cords[0], cords[1], cords[2]);
        
		fprintf(fw, "%d %d %d\n", color[0], color[1], color[2]);

		fprintf(fw, "%d ", track.size());
        for(int j=0;j<track.size();j++) {
            fprintf(fw, "%d ", track[j]);
            fprintf(fw, "%d ", sifts[point][j]);
            fprintf(fw, "%lf ", xs[point][j]);
            fprintf(fw, "%lf ", ys[point][j]); 
        }
        fprintf(fw,"\n");
    } 
	/*for (int i = 0; i < coverPoints.size(); i++) {
        fprintf(fw, "%d\n", coverPoints[i]);
    }*/
	fclose(fw);
	//fclose(f);
}

int main(int argc, char* argv[]) {
    int k = atoi(argv[3]);
    kcover mycover(argv[1], argv[2], k);
    mycover.ComputeCoverFast();
    mycover.WriteCover();

}
