#include "Bundle.h"
using namespace bundle;


float Bundle::findAngle
		(int key1, int image1, int key2, int image2, int pt){	
		double C1[] = {camSet[image1].t[0],camSet[image1].t[1],camSet[image1].t[2]};	
		double C2[] = {camSet[image2].t[0],camSet[image2].t[1],camSet[image2].t[2]};	
		double point[] = {vertexSet[pt].mPos[0], vertexSet[pt].mPos[1], vertexSet[pt].mPos[2]};

		double v1[] = {point[0]-C1[0], point[1]-C1[1], point[2]-C1[2]};
		double v2[] = {point[0]-C2[0], point[1]-C2[1], point[2]-C2[2]};
		
		double val = v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
		double norm1 = sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);		
		double norm2 = sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);		
		return (float)acos(val/(norm1*norm2));

}

float Bundle::calculateAngle(int image1, int image2) {
	vector< pair < int, int > > tempView1 = viewArr[image1];
	vector< pair < int, int > > tempView2 = viewArr[image2];

	map < int, int > temp_map;
	float angle = 0;
	int num_common = 0;
	for(int i=0;i<tempView1.size();i++)
	{
		int key = tempView1[i].first;	
		int pt = tempView1[i].second;
		temp_map[pt] = key;
	}
	for(int i=0;i<tempView2.size();i++)
	{

		int key2 = tempView1[i].first;	
		int pt = tempView1[i].second;
		if(temp_map.find(pt)!=temp_map.end())
		{
			int key1 = temp_map[pt];
			angle = angle + findAngle(key1,image1,key2,image2,pt);
			num_common++;

		}

	}
	if(num_common == 0){
		return -1;}
	return (angle*1.0f/(num_common*1.0f));
}

void Bundle::constructCorrMat(int dim) {
	corrMat.resize(dim, vector<int>(dim));
}
