#ifndef __BUNDLE_H
#define __BUNDLE_H
#include "defs.h"
//dummy declaration
namespace reader { class BundleReader;};

namespace bundle {


typedef struct CamStruct{
	double t[3];
	double R[9];
	double krd_inv[6]; // related to undistort
}CamStruct;

typedef struct Vertex {
	float mPos[3];
	float mColor[3];
	double mNumVis;
	vector< int > sift;
}Vertex;


class Vec3d {
	public:
		Vec3d() {}
		Vec3d(double, double, double);
		double el[3];
		double first() { return el[0]; }
		double second() { return el[1]; }
		double third() { return el[2]; }
};


class Bundle {
	int numImgs;
	int numPoints;

	vector< CamStruct > camSet;
	vector< Vec3d > camLocs;
	vector< Vec3d > camPos;
	vector< Vec3d > kf; 	
	vector< vector < double> > P_orig; //What is this?*/
	vector< vector < double> > k_inv;

	vector < Vertex > vertexSet;
	vector < vector < int > > pointArr;	
	vector < vector < pair <int, int> > > viewArr;

	//ViewInters Params
	vector< vector<int> > corrMat;

	//AngleArr Param
	vector< vector<float> > angleMat;

	float findAngle(int k1, int im1, int k2, int im2, int pt);
	float calculateAngle(int image1, int image2);

public:
	friend class reader::BundleReader;

	vector< bool > validTriangulated;
	void constructCorrMat(int dim);

    int getNumImages() {
        return numImgs;
    }


	const vector< int > & pointToImage(int idx) { 
		return pointArr[idx];
	}

	const vector< pair <int, int> >& viewArrRow(int idx) { 
		return viewArr[idx];
	}

	const vector< int > & corrMatRow(int idx) { 
		return corrMat[idx];
	}

	const vector< float > & angleMatRow(int idx) { 
		return angleMat[idx];
	}

	double* getP_orig(int idx) {
		return P_orig[idx].data();	
	}

	double* getk_inv(int idx) {
		return k_inv[idx].data();
	}

    const Vertex* getVertex(int idx) {
        return &vertexSet[idx];
    }

	const CamStruct* getCamSet(int idx) {
		return &camSet[idx];
	}

	int getNumImgs() { return numImgs; }
	int getNumPts() {return numPoints; }

	double getFocalLength(int idx) {
		return kf[idx].el[0];
	}

	double getR1(int idx) {
		return kf[idx].el[1];
	}

	double getR2(int idx) {
		return kf[idx].el[2];
	}

	Vec3d getCamPos(int idx) {
		return camPos[idx];
	}

    Vec3d getCamLocs(int idx) {
		return camLocs[idx];
	}
};

}

 
#endif //__BUNDLE_H 
