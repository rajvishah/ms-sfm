#include "defs.h"
#pragma once
class sift_img
{
	public:

		FILE *fp_key;
		cv::Mat img;
		int width;
		int height;
		int num_keys;

		unsigned char *keypoints ;

		void init(int num,char *query);
		void SetSift(char *path,std::vector<SiftGPU::SiftKeypoint> &keys1,std::vector<unsigned char > &descriptors1);
//		void ReadSiftFile(char *fp,char *query);
};


