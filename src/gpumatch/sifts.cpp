#include "defs.h"
#include "sifts.h"
void sift_img::SetSift(char *path,std::vector<SiftGPU::SiftKeypoint> &keys1,std::vector<unsigned char > &descriptors1)
{
	int mynum1 = 0, desc_size = 0;
	
	FILE *img1;

	img1 = fopen(path,"r");

	fscanf(img1,"%d",&mynum1);
	fscanf(img1,"%d",&desc_size);
	num_keys = mynum1;
//	keypoints = (unsigned char *) malloc(sizeof(unsigned char)*num_keys*128); 



	keys1.resize(mynum1);    
	descriptors1.resize(128*mynum1);
	for(int i = 0; i < mynum1; i++)
	{   
		fscanf(img1,"%f%f%f%f",&keys1[i].y,&keys1[i].x,&keys1[i].s,&keys1[i].o);
		for(int j = 0; j < 128; j++)
		{
			//fscanf(img1,"%f",&descriptors1[(i*128)+j]);
			//descriptors1[(i*128)+j] = (descriptors1[(i*128)+j])/512;
			fscanf(img1,"%hhu",&descriptors1[(i*128)+j]);
	//		descriptors1[(i*128)+j] = (descriptors1[(i*128)+j])/512;
		}

	}   
	num_keys = mynum1;
	fclose(img1);

}
void sift_img::init(int num,char *query)
{


	img = cv::imread(query,CV_LOAD_IMAGE_COLOR);
	cv::Size sz1 = img.size();
	width = sz1.width;
	height = sz1.height;
	num_keys = num;
	img.release();

	//--------------------------------------------------------------
	//==============================================================

}

/*
void sift_img::ReadSiftFile(char *fp,char *query)
{

	//====== Scan the details of source image from .key file ======

	fp_key = fopen ( fp , "r" );
	img = cv::imread(query,CV_LOAD_IMAGE_COLOR);
	cv::Size sz1 = img.size();
	width = sz1.width;
	height = sz1.height;

	fscanf(fp_key,"%d%d",&num_keys,&vector_size); 	//num_keys : num of features 

	//--------------------------------------------------------------
	//--------------- Allocate memory on host ----------------------

	siftID = (int *) malloc(sizeof(int)*num_keys);
//	siftID1 = (int *) malloc(sizeof(int)*num_keys);
//	siftID2 = (int *) malloc(sizeof(int)*num_keys);
//	siftID3 = (int *) malloc(sizeof(int)*num_keys);
//	siftID4 = (int *) malloc(sizeof(int)*num_keys);

	x = (float*) malloc(sizeof(float)*num_keys); 
	y = (float*) malloc(sizeof(float)*num_keys); 
	scale = (float*) malloc(sizeof(float)*num_keys); 
	orient = (float*) malloc(sizeof(float)*num_keys); 
	keypoints = (unsigned char *) malloc(sizeof(unsigned char)*num_keys*128); 


	//--------------------------------------------------------------
	//---------- Scan all features of source image -----------------

	int i,j;
	for(i = 0;i<num_keys;i++)
	{
		siftID[i] = i;
		fscanf(fp_key,"%f%f%f%f",&y[i],&x[i],&scale[i],&orient[i]);
		for(j = 0; j < 128; j++)
			fscanf(fp_key,"%hhu",&keypoints[i*128+j]);
	}
	fclose (fp_key);

	//--------------------------------------------------------------
	//==============================================================

}
*/
