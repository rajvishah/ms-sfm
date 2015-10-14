#include "defs.h"
#include "sifts.h"
#include "Gridder.h"
#include "Matcher.h"

#include <thrust/extrema.h>

//============================== DESCRIPTION =====================================
//   
//   This code deals with matching sifts between two images                       
//   This code implements the algorithm for GPU Matching. It is assumed that      
//   F-matrix is computed from the coarse k% features and remaining features are  
//   matched using this algorithm. We will match SIFT                             
//   features in the query/source image to those in the target image.             
//                                                                                
//================================================================================

//============================ Declare textures ==================================


texture <unsigned char, 1, cudaReadModeElementType> tex_sourceDescriptor;
texture <unsigned char, 1, cudaReadModeElementType> tex_targetDescriptor;


void Matcher::visualizeMatches(sift_img source,sift_img target,std::vector<std::pair<int,int> > matches,std::vector<SiftGPU::SiftKeypoint> &keys1,std::vector<SiftGPU::SiftKeypoint> &keys2) 
{
	const char* saveAs = NULL;
	cv::Mat queryImage = source.img;
	int qWidth = source.width;
	int qHeight = source.height;

	cv::Mat referenceImage = target.img;
	int rWidth = target.width;
	int rHeight = target.height;

	int canvasWidth = qWidth + rWidth;
	int canvasHeight = qHeight > rHeight ? qHeight : rHeight;
	cv::Mat canvas(canvasHeight, canvasWidth, CV_8UC3, cv::Scalar(0,0,0));

	cv::Rect roi1 = cv::Rect(0,0,qWidth,qHeight);
	cv::Mat canvas_roi1 = canvas(roi1);
	queryImage.copyTo(canvas_roi1);

	cv::Rect roi2 = cv::Rect(qWidth,0,rWidth,rHeight);
	cv::Mat canvas_roi2 = canvas(roi2);
	referenceImage.copyTo(canvas_roi2);

	for(int i=0; i < matches.size(); i++) {
		std::pair<int,int> p = matches[i];
		cv::Point pt1 = cv::Point(keys1[p.first].x, keys1[p.first].y);
		cv::Point pt2 = cv::Point(keys2[p.second].x + qWidth, keys2[p.second].y);

		cv::circle(canvas, pt1, 2, cv::Scalar(0,255,0), 4);
		cv::circle(canvas, pt2, 2, cv::Scalar(0,255,0), 4);
		cv::line(canvas, pt1, pt2, cv::Scalar(0,255,0), 4);
	}

	cv::namedWindow("FeatureMatches",cv::WINDOW_NORMAL);
	imshow("FeatureMatches", canvas);
	cv::waitKey();
	if(saveAs != NULL) {
		imwrite( saveAs, canvas );
	}
}


struct cast 
{
	__host__ __device__
		unsigned char operator()(float x) { return ((unsigned char)(512*x)); }
};

__global__ void computeGridID(SiftGPU::SiftKeypoint *keys, int *gridID1,int *gridID2,int *gridID3,int *gridID4, int num_keys,int gridSize,int numXGrids1,int halfSize,int numXGrids2,int numGrids,int numGridsXOv,int numGridsYOv)
{	
	//=========== Function to compute gridID given siftID ===========

	// Find data location to be processed by current thread
	int siftID = blockIdx.x*blockDim.x + threadIdx.x;	
	if(siftID < num_keys) 
	{
		double simpleX = ((double)keys[siftID].x/(double)gridSize);
		double simpleY = ((double)keys[siftID].y/(double)gridSize);

		double ov = (double)halfSize/gridSize;
		gridID1[siftID] = floor(simpleY)*numXGrids1 + floor(simpleX);
		gridID2[siftID] = floor(simpleY)*numXGrids2 + floor(simpleX - ov);
		gridID3[siftID] = floor(simpleY - ov)*numXGrids1 + floor(simpleX);
		gridID4[siftID] = floor(simpleY -ov)*numXGrids2 + floor(simpleX - ov);
		
		if(gridID2[siftID] < 0)
			gridID2[siftID] = 0;
		if(gridID3[siftID] < 0)
			gridID3[siftID] = 0;
		if(gridID4[siftID] < 0)
			gridID4[siftID] = 0;

		// gridID corresponding to the given siftID is computed
	}
	__syncthreads();

}


__global__ void findTopOfBucket(int *d_gridID,int *d_gridID2,int *d_gridID3,int *d_gridID4, int num_elements,int *d_topOfBucket_ptr,int *d_topOfBucket2_ptr,int *d_topOfBucket3_ptr,int *d_topOfBucket4_ptr,int numGrids,int numGridsXOv,int numGridsYOv,int numGridsXYOv)
{
	// Find data location to be processed by current thread
	int tidx = blockIdx.x*blockDim.x + threadIdx.x;	
	if(tidx < num_elements && tidx != 0) 
	{
		d_topOfBucket_ptr[d_gridID[0]] = 0;
		d_topOfBucket2_ptr[d_gridID2[0]] = 0;
		d_topOfBucket3_ptr[d_gridID3[0]] = 0;
		d_topOfBucket4_ptr[d_gridID4[0]] = 0;
		
		if (d_gridID[tidx] != d_gridID[tidx-1])
			d_topOfBucket_ptr[d_gridID[tidx]] = tidx;
		if (d_gridID2[tidx] != d_gridID2[tidx-1])
			d_topOfBucket2_ptr[d_gridID2[tidx]] = tidx;
		if (d_gridID3[tidx] != d_gridID3[tidx-1])
			d_topOfBucket3_ptr[d_gridID3[tidx]] = tidx;
		if (d_gridID4[tidx] != d_gridID4[tidx-1])
			d_topOfBucket4_ptr[d_gridID4[tidx]] = tidx;

	}
	__syncthreads();
}

__global__ void	findNumSift(int *gridID,int *topOfBucket,int *gridID2,int *topOfBucket2,int *gridID3,int *topOfBucket3,int *gridID4,int *topOfBucket4,int numGrids,int numGridsXOv,int numGridsYOv,int numGridsXYOv,int *numSift,int *numSift2,int *numSift3,int *numSift4)
{
	//===========  Find number of target sifts in each grid ===========

	// Find data location to be processed by current thread
	int gridid = blockIdx.x*blockDim.x + threadIdx.x;	
	int i,numsift = 0;
	if(gridid < numGrids)
	{
		if(topOfBucket[gridid]!=-1)
		{
			for(i = topOfBucket[gridid]; gridID[i] == gridid; i++)
				numsift++;
			numSift[gridid] = numsift;
		}
		else
			numSift[gridid] = 0;
	}
	if(gridid < numGridsXOv)
	{
		if(topOfBucket2[gridid]!=-1)
		{
			numsift = 0;
			for(i = topOfBucket2[gridid]; gridID2[i] == gridid; i++)
				numsift++;
			numSift2[gridid] = numsift;
		}
		else
			numSift2[gridid] = 0;
	}
	if(gridid < numGridsYOv)
	{
		if(topOfBucket3[gridid]!=-1)
		{
			numsift = 0;
			for(i = topOfBucket3[gridid]; gridID3[i] == gridid; i++)
				numsift++;
			numSift3[gridid] = numsift;
		}
		else
			numSift3[gridid] = 0;
	}
	if(gridid < numGridsXYOv)
	{
		if(topOfBucket4[gridid]!=-1)
		{
			numsift = 0;
			for(i = topOfBucket4[gridid]; gridID4[i] == gridid; i++)
				numsift++;
			numSift4[gridid] = numsift;
		}
		else
			numSift4[gridid] = 0;
	}
	__syncthreads();

}


__global__ void findEpipolarLine(float *fmatrix,SiftGPU::SiftKeypoint *keys,int source_width, int source_height,int width,int height,long long int *epipolarPoints,int source_num_keys)
{
	//============  Find Epipolar Line for each source sift ============

	int source_siftID = blockIdx.x*blockDim.x + threadIdx.x;	

	if(source_siftID < source_num_keys)
	{
		float epipolarLine[3];
		short x1,y1,x2,y2;		  // border points of line segment inside the target image
		float top_x, right_y, bottom_x, left_y ;    // points of intersection with the top , right , bottom and left lines of the target image border rectangle
		float x,y;

		// To Euclidian Coordinate system
		x = keys[source_siftID].x - ((source_width-1)/2);
		y = ((source_height-1)/2) - keys[source_siftID].y;

		// E = F.x
		epipolarLine[0] = (fmatrix[0]*x) + (fmatrix[1]*y) + fmatrix[2];
		epipolarLine[1] = (fmatrix[3]*x) + (fmatrix[4]*y) + fmatrix[5];
		epipolarLine[2] = (fmatrix[6]*x) + (fmatrix[7]*y) + fmatrix[8];
		//=====================================================================
		//
		//    In Euclidian Coordinate system (centrailized),
		//    line equations for target image rectangle: 
		//  ---------------------------------------------------------
		//    y=(h-1)/2    (top)
		//    x=(w-1)/2    (right)
		//    y=-(h-1)/2   (bottom)
		//    x=-(w-1)/2   (left)
		//
		//  ---------------------------------------------------------
		//
		//    So points of intersection with epipolar line ( ax + by + c = 0 ) are :
		//  ---------------------------------------------------------
		//    x = (-b(h-1)/2 -c )/a, y = (h-1)/2
		//    x = (w-1)/2, y = (-c-(w-1)a/2)/b
		//    x = (-c+(h-1)b/2)/a , y = -(h-1)/2
		//    x = -(w-1)/2 , y =( -c+(w-1)a/2)/b
		//    
		//=====================================================================

		//================== Find points of intersection ======================

		top_x = ((-1*epipolarLine[1]*(height-1)/2)-(1*epipolarLine[2]))/epipolarLine[0] ;
		right_y =((-1*epipolarLine[2])-((width-1)*epipolarLine[0]/2))/epipolarLine[1];
		bottom_x =((-1*epipolarLine[2])+((height-1)*epipolarLine[1]/2))/epipolarLine[0];
		left_y =((-1*epipolarLine[2])+((width-1)*epipolarLine[0]/2))/epipolarLine[1];

		//=====================================================================

		// Now these points (top , bottom ,left, right) are in Euclidian Coordinate
		// system ((0,0) at the centre of the image)
		// Back to graphics coordinate system ((0,0) at the top left corner of 
		// image and downwards is +ve y)

		top_x = top_x + ((width-1)/2);
		bottom_x = bottom_x + ((width-1)/2);
		right_y = ((height-1)/2) - right_y;
		left_y = ((height-1)/2) - left_y;

		//=====================================================================

		//=====================================================================
		// 
		//   -> Now only two of these points will lie on the target image border rectangle
		//   -> They are (x1,y1) and (x2,y2)
		//   -> Note : x1 < x2
		//
		//=====================================================================

		x1 = x2 = y1 = y2 = 0;
		//====================== Find x1, x2, y1, y2 ==========================

		if(left_y >= 0 && left_y <= height-1)
		{
			if(top_x >= 0 && top_x <= width-1)
			{
				x1 = 0;
				y1 = (short)left_y;
				x2 = (short)top_x;
				y2 = 0;
			}
			else if(bottom_x >= 0 && bottom_x <= width-1)
			{
				x1 = 0;
				y1 = (short)left_y;
				x2 = (short)bottom_x;
				y2 = height-1;
			}
			else if(right_y >= 0 && right_y <= height-1)
			{
				x1 = 0;
				y1 = (short)left_y;
				x2 = width-1;
				y2 = (short)right_y;
			}
		}
		else if(top_x >= 0 && top_x <= width-1)
		{
			if(right_y >= 0 && right_y <= height-1)
			{
				x1 = (short)top_x;
				y1 = 0;
				x2 = width-1;
				y2 = (short)right_y;
			}
			else if(bottom_x >= 0 && bottom_x <= width-1)
			{
				if(top_x < bottom_x)
				{
					x1 = (short)top_x;
					y1 = 0;
					x2 = (short)bottom_x;
					y2 = height-1;

				}
				else
				{
					x2 = (short)top_x;
					y2 = height-1;
					x1 = (short)bottom_x;
					y1 = 0;

				}
			}
		}
		else if(bottom_x >= 0 && bottom_x <= width-1)
		{
			if(right_y >= 0 && right_y <= height-1)
			{
				x1 = (short)bottom_x;
				y1 = height-1;
				x2 = width-1;
				y2 = (short)right_y;
			}

		}

		//=====================================================================

		//======= Packing all these points in a single long long int ==========

		long long int l = 0;
		l = l | (long long int)x1 << 48;
		l = l | (long long int)y1 << 32;
		l = l | (long long int)x2 << 16;
		l = l | (long long int)y2 << 0;
		epipolarPoints[source_siftID] = l;

		//=====================================================================
	}
	__syncthreads();
	// Done !
}


__global__ void clusterLines(long long int *d_epipolarPoints,int *cluster,int source_num_keys)
{

	//=====================================================================
	//
	//   d_epipolarPoints is a sorted array of packed points through which 
	//   we extract the end points of an epipolar line. If there is a 
	//   significant difference between two adjacent end points (say previous 
	//   and current), we mark the current one as 1 else 0 and store it in 
	//   cluster[current]. By default cluster[0] = 0. In this way all the 
	//   points between 1 to the next 1 fall in one group 
	//
	//=====================================================================

	int id = blockIdx.x*blockDim.x + threadIdx.x;	
	// id = 0 : first entry handled on the cpu cluster[0] = 0
	if( id < source_num_keys && id > 0)
	{
		unsigned long long int s = 0,l;
		short x1,y1,x2,y2;
		short prev_x1,prev_y1,prev_x2,prev_y2;
		s = ~s;
		s = s >> 48;

		// Extract the points from the packed point 
		l = d_epipolarPoints[id];
		x1 = (short)(s & (l >> 48));
		y1 = (short)(s & (l >> 32));
		x2 = (short)(s & (l >> 16));
		y2 = (short)(s & (l));

		// Extract the points from the packed point 
		l = d_epipolarPoints[id-1];
		prev_x1 = (short)(s & (l >> 48));
		prev_y1 = (short)(s & (l >> 32));
		prev_x2 = (short)(s & (l >> 16));
		prev_y2 = (short)(s & (l));

		if(((x1-prev_x1)*(x1-prev_x1) < 4) && ((y1-prev_y1)*(y1-prev_y1)< 4) && ((x2-prev_x2)*(x2-prev_x2) < 4) && ((y2-prev_y2)*(y2-prev_y2) < 4))  // If no significant difference between current and previous
		{
			cluster[id] = 0;

		}
		else
		{
			cluster[id] = 1;
		}
	}
	__syncthreads();
	// Done !

}


__global__ void findClusterLocation(int *cluster, int source_num_keys,int *clusterLocation,int *source_siftID,int *clusterID)
{
	//===================================================================
	//
	// Current scene example
	// +-----------+---+---+---+---+---+---+---+---+-----
	// | tidx      | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | ...
	// +-----------+---+---+---+---+---+---+---+---+-----
	// | siftid    | a | b | c | d | e | f | g | h |...
	// +-----------+---+---+---+---+---+---+---+---+-----
	// | cluster   | 0 | 1 | 1 | 2 | 3 | 3 | 3 | 4 |...
	// +-----------+---+---+---+---+---+---+---+---+-----
	//
	// So we want cluster location to be like this so that it 
	// is easy to find out members of one cluster
	// +------------------+---+---+---+---+---+--------
	// | cluster_id       | 0 | 1 | 2 | 3 | 4 | ...
	// +------------------+---+---+---+---+---+--------
	// | cluster_location | 0 | 1 | 3 | 4 | 7 | ...
	// +------------------+---+---+---+---+---+--------
	//
	//===================================================================


	// tidx = 0 handled on the CPU
	int tidx = blockIdx.x*blockDim.x + threadIdx.x;	
	if(tidx < source_num_keys)
	{
		if( tidx != 0) 
		{
			if (cluster[tidx] != cluster[tidx-1])
				clusterLocation[cluster[tidx]] = tidx;
		}
		clusterID[source_siftID[tidx]] = cluster[tidx];
	}
	__syncthreads();

}


__device__ void getBestGrid(float x, float y, int gridSize,int halfSize,int numXGrids1,int numXGrids2,int numGridsXOv,int numGridsYOv,int *gridid, int *which,int numGrids)
{
	int idx[4] = {0,0,0,0};
	float dists[4] = {0,0,0,0};


	double simpleX = ((double)x/gridSize);
	double simpleY = ((double)y/gridSize);


	double ov = (double)halfSize/gridSize;
	idx[0] = floor(simpleY)*numXGrids1 + floor(simpleX);
	idx[1] = floor(simpleY)*numXGrids2 + floor(simpleX - ov);
	idx[2] = floor(simpleY - ov)*numXGrids1 + floor(simpleX);
	idx[3] = floor(simpleY -ov)*numXGrids2 + floor(simpleX - ov);

	if(idx[1] < 0)
		idx[1] = 0;
	if(idx[2] < 0)
		idx[2] = 0;
	if(idx[3] < 0)
		idx[3] = 0;

	float g1Xc = (floor(simpleX))*gridSize + (float)halfSize;
	float g1Yc = (floor(simpleY))*gridSize + (float)halfSize;

	float g2Xc = (floor(simpleX - ov))*gridSize +(float) 2*halfSize;
	//float g2Yc = g1Yc;

	//float g3Xc = g1Xc;
	float g3Yc = (floor(simpleY - ov))*gridSize + (float)2*halfSize;

	//float g4Xc = g2Xc;
	//float g4Yc = g3Yc;

	float x1_d = (g1Xc - x)*(g1Xc - x); 
	float y1_d = (g1Yc - y)*(g1Yc - y);

	float x2_d = (g2Xc - x)*(g2Xc - x);
	float y2_d = (g3Yc - y)*(g3Yc - y);

	dists[0] = x1_d + y1_d;
	dists[1] = x2_d + y1_d;
	dists[2] = x1_d + y2_d;
	dists[3] = x2_d + y2_d;

	float min_g_dist = 200000;
	int minIdx = -1;
	int idno = 0;
	for(int id=0; id < 4; id++) {
		if(dists[id] < min_g_dist && idx[id] >= 0) {
			idno = id+1;
			min_g_dist = dists[id];
			minIdx = idx[id];
		}
	}

	*gridid = minIdx;
	*which = idno;
	//	*gridid = idx[0];
	//	*which = 1;


}




__global__ void	findNumPotentialMatchesForEachCluster(int *gridID,int *target_siftID,int *topOfBucket,int *gridID2,int *target_siftID2,int *topOfBucket2,int *gridID3,int *target_siftID3,int *topOfBucket3,int *gridID4,int *target_siftID4,int *topOfBucket4,int gridSize,int *clusterLocation,long long int *epipolarPoints,int *numSift,int *numSift2,int *numSift3,int *numSift4,int target_height,int numXGrids1,int halfSize,int numXGrids2,int numGridsXOv,int numGridsYOv,int numGrids,int numGridsXYOv,int *numPotentialMatches)	
{
	//=====================================================================
	//
	// For each cluster, load the epipolar line and mark eqidistant points
	// on that line. Now find the respective grids of these points.
	// After knowing which all grids are a part of the epipolar line, The 
	// potential matches are just the target sifts in those grids.
	//
	//=====================================================================

	int clusterID = blockIdx.x;
	int threadID = threadIdx.x;
	int clusterlocation = clusterLocation[clusterID];
	//potentialMatches[maxNumPotentialMatches*clusterID] = 0; 
	extern __shared__ int shared_array[];  

	int i,j,k;
	int distance, d, num_of_points, num_of_pts_per_thread;

	float cos_theta,sin_theta,x,y,slope;

	unsigned long long int s = 0,l;
	short x1,y1,x2,y2;

	s = ~s;
	s = s >> 48;
	l = epipolarPoints[clusterlocation];

	// Extract the points from the packed point l
	x1 = (short)(s & (l >> 48));
	y1 = (short)(s & (l >> 32));
	x2 = (short)(s & (l >> 16));
	y2 = (short)(s & (l));


	distance = (int) sqrt((float)((x1-x2)*(x1-x2)) + ((y1-y2)*(y1-y2)));
	d = gridSize ; 	// equidistant points at d distance
	num_of_points = ceil((float)distance/gridSize);

	if(distance > 5000)
	{
		num_of_points = (5000/gridSize);
		d = distance/num_of_points;
	}

	num_of_pts_per_thread = ceil((float)num_of_points/blockDim.x);


	if((x2-x1) != 0)	// slope is not infinity
	{
		slope = (float)(y2-y1)/(x2-x1);
		cos_theta = 1/sqrt(1+(slope*slope));
		sin_theta = slope/sqrt(1+(slope*slope));
	}
	else			// slope is infinite : vertical epipolar line
	{
		cos_theta = 0;
		sin_theta = 1;
	}

	// Compute gridIDs for num_of_pts_per_thread 

	//==========================================================================
	//
	//   Size of the following array = 10
	//   Reason : I assume that num_of_pts_per_thread can be 10 at max 
	//   if so then num_of_pts = 256(block_size)x10 = 2560
	//   assumption : min gridSize = 2 (4 actually :P )
	//   so length = 2x2560 = 5120 (we have already assumed max length to be 5000)
	//   therefore no problem with this assumption :D
	//  
	//==========================================================================

	int totalNumsift,top;
	int grids[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
	int which[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
	//int gridx,gridy,grid_id;
	int grid_id;

	totalNumsift = 0;
	top = -1;
	
	int which_grid = 0;
	for(i = 0 ; i < num_of_pts_per_thread ; i++ )
	{
		// threadID*num_of_pts_per_thread th point
		x = (float)(x1+(((threadID*num_of_pts_per_thread)+i)*cos_theta*d));
		y = (float)(y1+(((threadID*num_of_pts_per_thread)+i)*sin_theta*d));

		if(top == 10)
			break;

		if( x >= (float)x1 && x <= (float)x2 && y <= (float)target_height-1 && y >= 0.0)	
			// the point lies in the rectangle
		{
			// get best grid id

			// Compute gridID of the point
			getBestGrid(x, y,gridSize, halfSize,numXGrids1, numXGrids2, numGridsXOv,numGridsYOv,&grid_id,&which_grid,numGrids);
			 
			if(top == -1)
			{
				if(which_grid == 1)
				{
					totalNumsift += numSift[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 2)
				{
					totalNumsift += numSift2[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 3)
				{
					totalNumsift += numSift3[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 4)
				{
					totalNumsift += numSift4[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
			}
			else if(grid_id!=grids[top])
			{
				if(which_grid == 1)
				{
					totalNumsift += numSift[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 2)
				{
					totalNumsift += numSift2[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 3)
				{
					totalNumsift += numSift3[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 4)
				{
					totalNumsift += numSift4[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
			}
		}
	}

	shared_array[threadID] = totalNumsift;

	__syncthreads();	
	
	//=========================================================================
	// Find the number of target sifts each thread has to process
	// and write it into shared memory
	// to make sure that all the threads have written their num of sifts in the shared memory
	// Now we need to compute the prefix sum of this shared array which has 256 elements as of now
	// to know the position to write in the global memory for each thread
	//=========================== Prefix Sum ====================================

	//================================ Upsweep ==================================

	if(threadID < 128)
		shared_array[256+threadID] = shared_array[2*threadID]+shared_array[(2*threadID)+1];
	__syncthreads();

	if(threadID < 64)
		shared_array[256+128+threadID] = shared_array[256+(2*threadID)]+shared_array[256+(2*threadID)+1];
	__syncthreads();

	if(threadID < 32)
		shared_array[256+128+64+threadID] = shared_array[256+128+(2*threadID)]+shared_array[256+128+(2*threadID)+1];
	__syncthreads();

	if(threadID < 16)
		shared_array[256+128+64+32+threadID] = shared_array[256+128+64+(2*threadID)]+shared_array[256+128+64+(2*threadID)+1];
	__syncthreads();

	if(threadID < 8)
		shared_array[256+128+64+32+16+threadID] = shared_array[256+128+64+32+(2*threadID)]+shared_array[256+128+64+32+(2*threadID)+1];
	__syncthreads();

	if(threadID < 4)
		shared_array[256+128+64+32+16+8+threadID] = shared_array[256+128+64+32+16+(2*threadID)]+shared_array[256+128+64+32+16+(2*threadID)+1];
	__syncthreads();

	if(threadID < 2)
		shared_array[256+128+64+32+16+8+4+threadID] = shared_array[256+128+64+32+16+8+(2*threadID)]+shared_array[256+128+64+32+16+8+(2*threadID)+1];
	__syncthreads();

	if(threadID < 1)
		shared_array[256+128+64+32+16+8+4+2+threadID] = shared_array[256+128+64+32+16+8+4+(2*threadID)]+shared_array[256+128+64+32+16+8+4+(2*threadID)+1];
	__syncthreads();


	//======================== Downsweep ==============================

	if(threadID < 2 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+8+4+threadID] = shared_array[256+128+64+32+16+8+4+2+(threadID/2)];
		else
			shared_array[256+128+64+32+16+8+4+threadID] += shared_array[256+128+64+32+16+8+4+2+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 4 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+8+threadID] = shared_array[256+128+64+32+16+8+4+(threadID/2)];
		else
			shared_array[256+128+64+32+16+8+threadID] += shared_array[256+128+64+32+16+8+4+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 8 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+threadID] = shared_array[256+128+64+32+16+8+(threadID/2)];
		else
			shared_array[256+128+64+32+16+threadID] += shared_array[256+128+64+32+16+8+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 16 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+threadID] = shared_array[256+128+64+32+16+(threadID/2)];
		else
			shared_array[256+128+64+32+threadID] += shared_array[256+128+64+32+16+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 32 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+threadID] = shared_array[256+128+64+32+(threadID/2)];
		else
			shared_array[256+128+64+threadID] += shared_array[256+128+64+32+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 64 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+threadID] = shared_array[256+128+64+(threadID/2)];
		else
			shared_array[256+128+threadID] += shared_array[256+128+64+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 128 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+threadID] = shared_array[256+128+(threadID/2)];
		else
			shared_array[256+threadID] += shared_array[256+128+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 256 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[threadID] = shared_array[256+(threadID/2)];
		else
			shared_array[threadID] += shared_array[256+(threadID/2)-1];
	}
	__syncthreads();

	//==========================================================================
	//	
	// Now shared array[0..255] contains prefix sum
	// There is an array of size maxNumPotentialMatches for each cluster, we will write the potential matches there
	// The 1st entry would be the total number of potential matches and 
	// then each thread will write the matching sift ids it found at write position
	//
	//==========================================================================


	if(threadID == 0)
	{
//		printf("%d \n",shared_array[255]);
		numPotentialMatches[clusterID] = shared_array[255];
	}

	__syncthreads();

}



__global__ void	findPotentialMatchesForEachCluster(int *gridID,int *target_siftID,int *topOfBucket,int *gridID2,int *target_siftID2,int *topOfBucket2,int *gridID3,int *target_siftID3,int *topOfBucket3,int *gridID4,int *target_siftID4,int *topOfBucket4,int gridSize,int *clusterLocation,long long int *epipolarPoints,int *numSift,int *numSift2,int *numSift3,int *numSift4,int *potentialMatches,int target_height,int numXGrids1,int halfSize,int numXGrids2,int numGridsXOv,int numGridsYOv,int numGrids,int numGridsXYOv,int maxNumPotentialMatches)	
{
	//=====================================================================
	//
	// For each cluster, load the epipolar line and mark eqidistant points
	// on that line. Now find the respective grids of these points.
	// After knowing which all grids are a part of the epipolar line, The 
	// potential matches are just the target sifts in those grids.
	//
	//=====================================================================

	int clusterID = blockIdx.x;
	int threadID = threadIdx.x;
	int clusterlocation = clusterLocation[clusterID];
	potentialMatches[(2+maxNumPotentialMatches)*clusterID] = 0; 
	extern __shared__ int shared_array[];  

	int i,j,k;
	int distance, d, num_of_points, num_of_pts_per_thread;

	float cos_theta,sin_theta,x,y,slope;

	unsigned long long int s = 0,l;
	short x1,y1,x2,y2;

	s = ~s;
	s = s >> 48;
	l = epipolarPoints[clusterlocation];

	// Extract the points from the packed point l
	x1 = (short)(s & (l >> 48));
	y1 = (short)(s & (l >> 32));
	x2 = (short)(s & (l >> 16));
	y2 = (short)(s & (l));


	distance = (int) sqrt((float)((x1-x2)*(x1-x2)) + ((y1-y2)*(y1-y2)));
	d = gridSize ; 	// equidistant points at d distance
	num_of_points = ceil((float)distance/gridSize);

	if(distance > 5000)
	{
		num_of_points = (5000/gridSize);
		d = distance/num_of_points;
	}

	num_of_pts_per_thread = ceil((float)num_of_points/blockDim.x);


	if((x2-x1) != 0)	// slope is not infinity
	{
		slope = (float)(y2-y1)/(x2-x1);
		cos_theta = 1/sqrt(1+(slope*slope));
		sin_theta = slope/sqrt(1+(slope*slope));
	}
	else			// slope is infinite : vertical epipolar line
	{
		cos_theta = 0;
		sin_theta = 1;
	}

	// Compute gridIDs for num_of_pts_per_thread 

	//==========================================================================
	//
	//   Size of the following array = 10
	//   Reason : I assume that num_of_pts_per_thread can be 10 at max 
	//   if so then num_of_pts = 256(block_size)x10 = 2560
	//   assumption : min gridSize = 2 (4 actually :P )
	//   so length = 2x2560 = 5120 (we have already assumed max length to be 5000)
	//   therefore no problem with this assumption :D
	//  
	//==========================================================================

	int totalNumsift,top;
	int grids[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
	int which[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
	//int gridx,gridy,grid_id;
	int grid_id;

	totalNumsift = 0;
	top = -1;
	
	int which_grid = 0;
	for(i = 0 ; i < num_of_pts_per_thread ; i++ )
	{
		// threadID*num_of_pts_per_thread th point
		x = (float)(x1+(((threadID*num_of_pts_per_thread)+i)*cos_theta*d));
		y = (float)(y1+(((threadID*num_of_pts_per_thread)+i)*sin_theta*d));
		if(top == 10)
			break;

		if( x >= (float)x1 && x <= (float)x2 && y <= (float)target_height-1 && y >= 0.0)	
			// the point lies in the rectangle
		{
			// get best grid id

			// Compute gridID of the point
			getBestGrid(x, y,gridSize, halfSize,numXGrids1, numXGrids2, numGridsXOv,numGridsYOv,&grid_id,&which_grid,numGrids);
			 
			if(top == -1)
			{
				if(which_grid == 1)
				{
					totalNumsift += numSift[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 2)
				{
					totalNumsift += numSift2[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 3)
				{
					totalNumsift += numSift3[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 4)
				{
					totalNumsift += numSift4[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
			}
			else if(grid_id!=grids[top])
			{
				if(which_grid == 1)
				{
					totalNumsift += numSift[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 2)
				{
					totalNumsift += numSift2[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 3)
				{
					totalNumsift += numSift3[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
				else if(which_grid == 4)
				{
					totalNumsift += numSift4[grid_id];
					grids[++top] = grid_id;
					which[top] = which_grid;
				}
			}
		}
	}

	shared_array[threadID] = totalNumsift;

	__syncthreads();	
	
	//=========================================================================
	// Find the number of target sifts each thread has to process
	// and write it into shared memory
	// to make sure that all the threads have written their num of sifts in the shared memory
	// Now we need to compute the prefix sum of this shared array which has 256 elements as of now
	// to know the position to write in the global memory for each thread
	//=========================== Prefix Sum ====================================

	//================================ Upsweep ==================================

	if(threadID < 128)
		shared_array[256+threadID] = shared_array[2*threadID]+shared_array[(2*threadID)+1];
	__syncthreads();

	if(threadID < 64)
		shared_array[256+128+threadID] = shared_array[256+(2*threadID)]+shared_array[256+(2*threadID)+1];
	__syncthreads();

	if(threadID < 32)
		shared_array[256+128+64+threadID] = shared_array[256+128+(2*threadID)]+shared_array[256+128+(2*threadID)+1];
	__syncthreads();

	if(threadID < 16)
		shared_array[256+128+64+32+threadID] = shared_array[256+128+64+(2*threadID)]+shared_array[256+128+64+(2*threadID)+1];
	__syncthreads();

	if(threadID < 8)
		shared_array[256+128+64+32+16+threadID] = shared_array[256+128+64+32+(2*threadID)]+shared_array[256+128+64+32+(2*threadID)+1];
	__syncthreads();

	if(threadID < 4)
		shared_array[256+128+64+32+16+8+threadID] = shared_array[256+128+64+32+16+(2*threadID)]+shared_array[256+128+64+32+16+(2*threadID)+1];
	__syncthreads();

	if(threadID < 2)
		shared_array[256+128+64+32+16+8+4+threadID] = shared_array[256+128+64+32+16+8+(2*threadID)]+shared_array[256+128+64+32+16+8+(2*threadID)+1];
	__syncthreads();

	if(threadID < 1)
		shared_array[256+128+64+32+16+8+4+2+threadID] = shared_array[256+128+64+32+16+8+4+(2*threadID)]+shared_array[256+128+64+32+16+8+4+(2*threadID)+1];
	__syncthreads();


	//======================== Downsweep ==============================

	if(threadID < 2 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+8+4+threadID] = shared_array[256+128+64+32+16+8+4+2+(threadID/2)];
		else
			shared_array[256+128+64+32+16+8+4+threadID] += shared_array[256+128+64+32+16+8+4+2+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 4 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+8+threadID] = shared_array[256+128+64+32+16+8+4+(threadID/2)];
		else
			shared_array[256+128+64+32+16+8+threadID] += shared_array[256+128+64+32+16+8+4+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 8 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+16+threadID] = shared_array[256+128+64+32+16+8+(threadID/2)];
		else
			shared_array[256+128+64+32+16+threadID] += shared_array[256+128+64+32+16+8+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 16 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+32+threadID] = shared_array[256+128+64+32+16+(threadID/2)];
		else
			shared_array[256+128+64+32+threadID] += shared_array[256+128+64+32+16+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 32 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+64+threadID] = shared_array[256+128+64+32+(threadID/2)];
		else
			shared_array[256+128+64+threadID] += shared_array[256+128+64+32+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 64 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+128+threadID] = shared_array[256+128+64+(threadID/2)];
		else
			shared_array[256+128+threadID] += shared_array[256+128+64+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 128 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[256+threadID] = shared_array[256+128+(threadID/2)];
		else
			shared_array[256+threadID] += shared_array[256+128+(threadID/2)-1];
	}
	__syncthreads();

	if(threadID < 256 && threadID!=0)	
	{
		if(threadID%2 != 0)
			shared_array[threadID] = shared_array[256+(threadID/2)];
		else
			shared_array[threadID] += shared_array[256+(threadID/2)-1];
	}
	__syncthreads();


	//==========================================================================
	//	
	// Now shared array[0..255] contains prefix sum
	// There is an array of size maxNumPotentialMatches for each cluster, we will write the potential matches there
	// The 1st entry would be the total number of potential matches and 
	// then each thread will write the matching sift ids it found at write position
	//
	//==========================================================================

	potentialMatches[(2+maxNumPotentialMatches)*clusterID] = shared_array[255];	// total num of potential matches

	int writePosition = 1;

	if(threadID > 0)
		writePosition = shared_array[threadID-1] + 1;

	__syncthreads();



	k = 0;

	for(i=0;i<=top;i++)
	{
		if(which[i] == 1)
		{
			if(topOfBucket[grids[i]]!=-1)
			{
				for(j = topOfBucket[grids[i]]; grids[i] == gridID[j]; j++)
				{
					potentialMatches[((2+maxNumPotentialMatches)*clusterID)+writePosition+k] = target_siftID[j];
					k++;
				}

			}
		}
		else if(which[i] == 2)
		{
			if(topOfBucket2[grids[i]]!=-1)
			{
				for(j = topOfBucket2[grids[i]]; grids[i] == gridID2[j]; j++)
				{
					potentialMatches[((2+maxNumPotentialMatches)*clusterID)+writePosition+k] = target_siftID2[j];
					k++;
				}

			}
		}
		else if(which[i] == 3)
		{
			if(topOfBucket3[grids[i]]!=-1)
			{
				for(j = topOfBucket3[grids[i]]; grids[i] == gridID3[j]; j++)
				{
					potentialMatches[((2+maxNumPotentialMatches)*clusterID)+writePosition+k] = target_siftID3[j];
					k++;
				}

			}
		}
		else if(which[i] == 4)
		{
			if(topOfBucket4[grids[i]]!=-1)
			{
				for(j = topOfBucket4[grids[i]]; grids[i] == gridID4[j]; j++)
				{
					potentialMatches[((2+maxNumPotentialMatches)*clusterID)+writePosition+k] = target_siftID4[j];
					k++;
				}

			}
		}
	}
	__syncthreads();

}



__device__ void findmin1min2(int a, int b , int c, int d,int a_id,int b_id,int c_id,int d_id,int *min1,int *min2,int *min1_id,int *min2_id)
{
	//========================================================================
	// This chunk of code finds the minimum and next minimun and correnponding 
	// minimum and next minimum ID between a,b,c,d 
	//========================================================================

	if( a < b && a < c && a < d )
	{
		*min1 = a;
		*min1_id = a_id;
		if(b < c && b < d)
		{
			*min2 = b;
			*min2_id = b_id;
		}
		else if(c < d)
		{
			*min2 = c;
			*min2_id = c_id;
		}
		else
		{
			*min2 = d;
			*min2_id = d_id;
		}
	}
	else if(b < c && b < d)
	{
		*min1 = b;
		*min1_id = b_id;
		if(a < c && a < d)
		{
			*min2 = a;
			*min2_id = a_id;
		}
		else if(c < d)
		{
			*min2 = c;
			*min2_id = c_id;
		}
		else
		{
			*min2 = d;
			*min2_id = d_id;
		}
	}
	else if(c < d)
	{
		*min1 = c;
		*min1_id = c_id;
		if(a < b && a < d)
		{
			*min2 = a;
			*min2_id = a_id;
		}
		else if(b < d)
		{
			*min2 = b;
			*min2_id = b_id;
		}
		else
		{
			*min2 = d;
			*min2_id = d_id;
		}
	}
	else
	{
		*min1 = d;
		*min1_id = d_id;
		if(a < b && a < c)
		{
			*min2 = a;
			*min2_id = a_id;
		}
		else if(b < c)
		{
			*min2 = b;
			*min2_id = b_id;
		}
		else
		{
			*min2 = c;
			*min2_id = c_id;
		}
	}


}



__global__ void	findMatches(int *matches,int *clusterID,int *potentialMatches,int maxNumPotentialMatches)
{
	//========================================================================
	//
	// For each source sift, find the cluster it belongs to 
	// Then load the potential matching target sifts of that cluster and 
	// compute the L2 distance between the descriptors.
	// Now find the min and next min of these distances.
	// Declare a match based on some tests.
	//
	//========================================================================

	int source_siftID,threadID,cluster_id,num_potential_matches,num_pts_per_thread,target_siftID;
	int min_id,next_min_id,i,j;
	int min_num,next_min,dist;

	extern __shared__ int shared_array[];  

	source_siftID = blockIdx.x;
	threadID = threadIdx.x;
	cluster_id = clusterID[source_siftID];
	num_potential_matches = potentialMatches[(2+maxNumPotentialMatches)*cluster_id];

//	matches[source_siftID] = -1;

	if (num_potential_matches%blockDim.x == 0)
		num_pts_per_thread = num_potential_matches/blockDim.x;
	else
		num_pts_per_thread = 1 + (num_potential_matches/blockDim.x);


//	if(num_potential_matches > maxNumPotentialMatches)
//		printf("%d\n",num_potential_matches);

	min_num = 999999;
	next_min = 999999;
	min_id = -1;
	next_min_id = -1;
	int dummy;
	int a,b;

	for(i = 0;i < num_pts_per_thread; i++)
	{
		if((num_pts_per_thread*threadID)+i < num_potential_matches)
		{
			target_siftID = potentialMatches[((2+maxNumPotentialMatches)*cluster_id)+1+(num_pts_per_thread*threadID)+i];
			dist = 0;
			for(j = 0 ; j < 128; j++)
			{
				a = tex1Dfetch(tex_targetDescriptor,(target_siftID*128)+j);
				b = tex1Dfetch(tex_sourceDescriptor,(source_siftID*128)+j);
				dummy = a-b;
				dist+=(dummy*dummy);

			}

	
//			for( j = 0 ; j < 128 ; j++) 
//			{
//				dist+= (((int)tex1Dfetch(tex_sourceDescriptor,(source_siftID*128)+j) - ((int)tex1Dfetch(tex_targetDescriptor,(target_siftID*128)+j)))*((int)tex1Dfetch(tex_sourceDescriptor,(source_siftID*128)+j) - ((int)tex1Dfetch(tex_targetDescriptor,(target_siftID*128)+j))));
//			}

			if(dist <= min_num)
			{
				next_min = min_num;
				next_min_id = min_id;
				min_num = dist;
				min_id = target_siftID;
			}
			else if(dist < next_min)
			{
				next_min = dist;
				next_min_id = target_siftID;
			}
		}
	}

	// Store the min and next min found by each thread into the shared memory

	shared_array[threadID] = min_num;
	shared_array[threadID+blockDim.x] = next_min;
	shared_array[threadID+2*blockDim.x] = min_id;
	shared_array[threadID+3*blockDim.x] = next_min_id;
	__syncthreads();

	// Shared array fully occupied
	// Time to compute the min and next min across the block ( min and next min in the shared memory )

	int c,d;
	int a_id,b_id,c_id,d_id;
	int min1,min2;
	int min1_id,min2_id;

	if(threadID < 128)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+128];
		d = shared_array[threadID+blockDim.x+128];
		c_id = shared_array[threadID+2*blockDim.x+128];
		d_id = shared_array[threadID+3*blockDim.x+128];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 64)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+64];
		d = shared_array[threadID+blockDim.x+64];
		c_id = shared_array[threadID+2*blockDim.x+64];
		d_id = shared_array[threadID+3*blockDim.x+64];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 32)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+32];
		d = shared_array[threadID+blockDim.x+32];
		c_id = shared_array[threadID+2*blockDim.x+32];
		d_id = shared_array[threadID+3*blockDim.x+32];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 16)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+16];
		d = shared_array[threadID+blockDim.x+16];
		c_id = shared_array[threadID+2*blockDim.x+16];
		d_id = shared_array[threadID+3*blockDim.x+16];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 8)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+8];
		d = shared_array[threadID+blockDim.x+8];
		c_id = shared_array[threadID+2*blockDim.x+8];
		d_id = shared_array[threadID+3*blockDim.x+8];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 4)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+4];
		d = shared_array[threadID+blockDim.x+4];
		c_id = shared_array[threadID+2*blockDim.x+4];
		d_id = shared_array[threadID+3*blockDim.x+4];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID < 2)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+2];
		d = shared_array[threadID+blockDim.x+2];
		c_id = shared_array[threadID+2*blockDim.x+2];
		d_id = shared_array[threadID+3*blockDim.x+2];

		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		shared_array[threadID] = min1;
		shared_array[threadID+blockDim.x] = min2;
		shared_array[threadID+2*blockDim.x] = min1_id;
		shared_array[threadID+3*blockDim.x] = min2_id;
	}
	__syncthreads();

	if(threadID == 0)
	{
		a = shared_array[threadID];
		b = shared_array[threadID+blockDim.x];
		a_id = shared_array[threadID+2*blockDim.x];
		b_id = shared_array[threadID+3*blockDim.x];

		c = shared_array[threadID+1];
		d = shared_array[threadID+blockDim.x+1];
		c_id = shared_array[threadID+2*blockDim.x+1];
		d_id = shared_array[threadID+3*blockDim.x+1];

		// Compute min and next min
		findmin1min2(a,b,c,d,a_id,b_id,c_id,d_id,&min1,&min2,&min1_id,&min2_id);

		// Ratio test
		if((float)min1/min2 < 0.36 && min1_id > 0 && num_potential_matches > 9 && min2_id != 999999)	// we store the squared distances so the ratio becomes 0.36 instead of 0.6
		{
			// Declare match
			matches[source_siftID] = min1_id;

		}
	}
	__syncthreads();

}

int max(int a,int b,int c,int d)
{
	int maxim = -9999;
	if(maxim < a)
		maxim = a;
	if(maxim < b)
		maxim = b;
	if(maxim < c)
		maxim = c;
	if(maxim < d)
		maxim = d;
	return maxim;

}


double Matcher::matchImagePair(sift_img source,sift_img target,float *f_matrix,SiftGPU::SiftKeypoint* &d_keys1,unsigned char* &descriptors1 ,SiftGPU::SiftKeypoint* &d_keys2,unsigned char* &descriptors2 )
{

	struct timespec t1, t2, t3, t4, t5;

	clock_gettime(CLOCK_MONOTONIC, &t3);
	clock_gettime(CLOCK_MONOTONIC, &t1);

	//================== GPU initialization =======================

	cudaSetDevice(0);
	cudaFree(0);

	//=============================================================

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_gpu_init = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//=============================================================

	//=============================================================

	clock_gettime(CLOCK_MONOTONIC, &t1);
	//================== Declare variables ========================


	int i,j;
	int *h_cluster, *h_gridID;
	long long int *h_epipolarPoints; // Stores ends of the epipolar line segment packed in a single long long int

	h_epipolarPoints = (long long int*) malloc(sizeof(long long int)*source.num_keys); 
	h_cluster = (int *) malloc(sizeof(int)*source.num_keys);
	h_gridID = (int *) malloc(sizeof(int)*target.num_keys);	//to store gridIDs on host

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_total_cpu_alloc = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	//==================== Declare device arrays ===================

	clock_gettime(CLOCK_MONOTONIC, &t5);
//	float *d_target_x, *d_target_y, *d_source_x, *d_source_y;
//	SiftGPU::SiftKeypoint *d_keys1, *d_keys2;


	float *d_fmatrix;
//	float *desc1,*desc2;
	int *d_gridID , *d_target_siftID , *d_gridID1 , *d_target_siftID1, *d_gridID2 , *d_target_siftID2,*d_gridID3 , *d_target_siftID3,*d_gridID4 , *d_target_siftID4,   *d_source_siftID, *d_cluster, *d_clusterID;
////	unsigned char *d_source_keypoints,*d_target_keypoints;
	long long int *d_epipolarPoints;

	//==============================================================

	//=================== Allocate memory on device ================

//	cudaMalloc((void**)&d_keys1, sizeof(SiftGPU::SiftKeypoint)*source.num_keys);
//	cudaMalloc((void**)&d_keys2, sizeof(SiftGPU::SiftKeypoint)*target.num_keys);
//	cudaMalloc((void**)&desc1, 128*sizeof(float)*source.num_keys);
//	cudaMalloc((void**)&desc2, 128*sizeof(float)*target.num_keys);
//	cudaMalloc((void**)&d_source_x, sizeof(float)*source.num_keys);
//	cudaMalloc((void**)&d_source_y, sizeof(float)*source.num_keys);

//	cudaMalloc((void**)&d_target_x, sizeof(float)*target.num_keys);
//	cudaMalloc((void**)&d_target_y, sizeof(float)*target.num_keys);

////	cudaMalloc((void**)&d_source_keypoints, sizeof(unsigned char)*128*source.num_keys);
////	cudaMalloc((void**)&d_target_keypoints, sizeof(unsigned char)*128*target.num_keys);

	cudaMalloc((void**)&d_source_siftID, sizeof(int)*source.num_keys);
	cudaMalloc((void**)&d_target_siftID, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_target_siftID1, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_target_siftID2, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_target_siftID3, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_target_siftID4, sizeof(int)*target.num_keys);

	cudaMalloc((void**)&d_fmatrix, sizeof(float)*9);

	cudaMalloc((void**)&d_gridID, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_gridID1, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_gridID2, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_gridID3, sizeof(int)*target.num_keys);
	cudaMalloc((void**)&d_gridID4, sizeof(int)*target.num_keys);

	cudaMalloc((void**)&d_epipolarPoints, sizeof(long long int)*source.num_keys);

	cudaMalloc((void**)&d_cluster, sizeof(int)*source.num_keys);
	cudaMalloc((void**)&d_clusterID, sizeof(int)*source.num_keys);


	//==============================================================

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_malloc1 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);
	//================ Copy arrays from host to device =============



//	cudaMemcpy(desc1, descriptors1.data(), sizeof(float)*source.num_keys*128, cudaMemcpyHostToDevice); 
//	cudaMemcpy(desc2, descriptors2.data(), sizeof(float)*target.num_keys*128, cudaMemcpyHostToDevice); 
	clock_gettime(CLOCK_MONOTONIC, &t1);

//	cudaMemcpy(d_keys1, keys1.data(), sizeof(SiftGPU::SiftKeypoint)*source.num_keys, cudaMemcpyHostToDevice); 
//	cudaMemcpy(d_keys2, keys2.data(), sizeof(SiftGPU::SiftKeypoint)*target.num_keys, cudaMemcpyHostToDevice); 

	cudaMemcpy(d_fmatrix, f_matrix, sizeof(float)*9, cudaMemcpyHostToDevice); 

//	cudaMemcpy(d_target_keypoints, target.keypoints, sizeof(unsigned char)*target.num_keys*128, cudaMemcpyHostToDevice); 
//	cudaMemcpy(d_source_keypoints, source.keypoints, sizeof(unsigned char)*source.num_keys*128, cudaMemcpyHostToDevice); 

	//==============================================================

	int threadsPerBlock;
	int numBlocks;
	thrust::device_ptr<int> d_target_sift_ptr(d_target_siftID);
	thrust::device_ptr<int> d_source_sift_ptr(d_source_siftID);
	thrust::sequence(d_target_sift_ptr,d_target_sift_ptr+target.num_keys);
	thrust::sequence(d_source_sift_ptr,d_source_sift_ptr+source.num_keys);
	
	// initialize a device_vector with the list
////	thrust::host_vector<unsigned char> h_d1(descriptors1.begin(), descriptors1.end());
////	thrust::host_vector<unsigned char> h_d2(descriptors2.begin(), descriptors2.end());
////	thrust::device_vector<unsigned char> d1 = h_d1;
////	thrust::device_vector<unsigned char> d2 = h_d2; 
//	thrust::device_ptr<unsigned char> d_desc1_ptr(desc1);
//	thrust::device_ptr<unsigned char> d_desc2_ptr(desc2);
//	thrust::device_vector<unsigned char> u_d1(128*source.num_keys);
//	thrust::device_vector<unsigned char> u_d2(128*target.num_keys);
//	thrust::transform(d1.begin(), d1.end(), u_d1.begin(), cast());
//	thrust::transform(d2.begin(), d2.end(), u_d2.begin(), cast());

//	thrust::transform(d_desc1_ptr,d_desc1_ptr + 128*source.num_keys, u_d1.begin(), cast());
//	thrust::transform(d_desc2_ptr,d_desc2_ptr + 128*target.num_keys, u_d2.begin(), cast());

////	unsigned char * desc1_ptr = thrust::raw_pointer_cast(d1.data());
////	unsigned char * desc2_ptr = thrust::raw_pointer_cast(d2.data());

//	unsigned char *h = (unsigned char *)malloc(128*target.num_keys*sizeof(unsigned char));
//	unsigned char * dv_ptr = thrust::raw_pointer_cast(u_d2.data());
//	cudaMemcpy(h,dv_ptr,128*sizeof(unsigned char)*target.num_keys, cudaMemcpyDeviceToHost);

//	for(int i = 0; i < 128*target.num_keys; i++)
//	{
//		printf("%hhu\n",h[i]);
//	}



	cudaMemcpy(d_target_siftID1, d_target_siftID, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice); 
	cudaMemcpy(d_target_siftID2, d_target_siftID, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice); 
	cudaMemcpy(d_target_siftID3, d_target_siftID, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice); 
	cudaMemcpy(d_target_siftID4, d_target_siftID, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice); 

	//====================== Bind textures =========================

	cudaBindTexture (0, tex_targetDescriptor, descriptors2, sizeof(unsigned char)*target.num_keys*128 );
	cudaBindTexture (0, tex_sourceDescriptor, descriptors1, sizeof(unsigned char)*source.num_keys*128 );

//	cudaBindTexture (0, tex_targetDescriptor, d_target_keypoints, sizeof(unsigned char)*target.num_keys*128 );
//	cudaBindTexture (0, tex_sourceDescriptor, d_source_keypoints, sizeof(unsigned char)*source.num_keys*128 );

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_memcpy1 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);
	//==============================================================


	//printf("%d %d\n",source.num_keys,target.num_keys);

	//======================  INDEXING STEP ========================
	// The indexing step divides the target image into grids. 
	// The features get indexed based on the grid to which they belong.
	//==============================================================

	Grid target_grid;
	target_grid.g_init(target);

	//================ Decide kernel configuration =================

	threadsPerBlock = 512;
	numBlocks = ceil(((float)target.num_keys)/threadsPerBlock);

	//===================== Call the GPU Kernel ====================
	// This kernel computes gridID for each sift of the target image

	clock_gettime(CLOCK_MONOTONIC, &t1);

	
	computeGridID<<<numBlocks, threadsPerBlock>>>(d_keys2, d_gridID1, d_gridID2, d_gridID3, d_gridID4, target.num_keys,target_grid.gridSize, target_grid.numXGrids1,target_grid.halfSize,target_grid.numXGrids2,target_grid.numGrids,target_grid.numGridsXOv,target_grid.numGridsYOv);
	
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_computeGridID = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================


	// Current situation 
	// +-------+---+---+---+---+---+---+---+----
	// |SiftID | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ...
	// +-------+---+---+---+---+---+---+---+----
	// |GridID | a | b | c | d | e | f | g | ...
	// +-------+---+---+---+---+---+---+---+----
	// Desired situation
	// +-------+---+---+---+---+---+---+---+----
	// |GridID | 0 | 0 | 2 | 5 | 5 | 5 | 6 | ...
	// +-------+---+---+---+---+---+---+---+----
	// |SiftID | a | b | c | d | e | f | g | ...
	// +-------+---+---+---+---+---+---+---+----
	// Sort to bring sifts with same gridID together

	//========= Sort : keys = gridID , values = siftID ===========

	clock_gettime(CLOCK_MONOTONIC, &t1);

	thrust::device_ptr<int> d_gridID_ptr1(d_gridID1);
	thrust::device_ptr<int> d_target_siftID_ptr1(d_target_siftID1);

	thrust::device_ptr<int> d_gridID_ptr2(d_gridID2);
	thrust::device_ptr<int> d_target_siftID_ptr2(d_target_siftID2);

	thrust::device_ptr<int> d_gridID_ptr3(d_gridID3);
	thrust::device_ptr<int> d_target_siftID_ptr3(d_target_siftID3);

	thrust::device_ptr<int> d_gridID_ptr4(d_gridID4);
	thrust::device_ptr<int> d_target_siftID_ptr4(d_target_siftID4);

	thrust::sort_by_key(d_gridID_ptr1, d_gridID_ptr1+target.num_keys,d_target_siftID_ptr1);
	thrust::sort_by_key(d_gridID_ptr2, d_gridID_ptr2+target.num_keys,d_target_siftID_ptr2);
	thrust::sort_by_key(d_gridID_ptr3, d_gridID_ptr3+target.num_keys,d_target_siftID_ptr3);
	thrust::sort_by_key(d_gridID_ptr4, d_gridID_ptr4+target.num_keys,d_target_siftID_ptr4);

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust1 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);


	// Current situation 
	// +-------+---+---+---+---+---+---+---+----
	// |GridID | 0 | 0 | 2 | 5 | 5 | 5 | 6 | ...
	// +-------+---+---+---+---+---+---+---+----
	// |SiftID | a | b | c | d | e | f | g | ...
	// +-------+---+---+---+---+---+---+---+----
	// Now to access all sifts corresponding to gridID = x we record topOfBucket 
	// +---------------+---+---+---+---+---+---+---+---
	// |GridID         | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ...
	// +---------------+---+---+---+---+---+---+---+---
	// |TopOfBucket    | 0 |-1 | 2 |-1 |-1 | 3 | 6 | ...
	// +---------------+---+---+---+---+---+---+---+---

	// filhal
	cudaMemcpy(d_gridID, d_gridID1, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice);
	cudaMemcpy(d_target_siftID, d_target_siftID1, sizeof(int)*target.num_keys, cudaMemcpyDeviceToDevice);

	//================== Record topOfBucket =====================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	thrust::device_vector<int> d_topOfBucket(target_grid.numGrids, -1);
	thrust::device_vector<int> d_numSift(target_grid.numGrids, 0);

	thrust::device_vector<int> d_topOfBucket2(target_grid.numGridsXOv, -1);
	thrust::device_vector<int> d_numSift2(target_grid.numGridsXOv, 0);

	thrust::device_vector<int> d_topOfBucket3(target_grid.numGridsYOv, -1);
	thrust::device_vector<int> d_numSift3(target_grid.numGridsYOv, 0);

	thrust::device_vector<int> d_topOfBucket4(target_grid.numGridsXYOv, -1);
	thrust::device_vector<int> d_numSift4(target_grid.numGridsXYOv, 0);

	int *d_topOfBucket_ptr = thrust::raw_pointer_cast(d_topOfBucket.data());
	int *d_numSift_ptr = thrust::raw_pointer_cast(d_numSift.data());

	int *d_topOfBucket2_ptr = thrust::raw_pointer_cast(d_topOfBucket2.data());
	int *d_numSift2_ptr = thrust::raw_pointer_cast(d_numSift2.data());

	int *d_topOfBucket3_ptr = thrust::raw_pointer_cast(d_topOfBucket3.data());
	int *d_numSift3_ptr = thrust::raw_pointer_cast(d_numSift3.data());

	int *d_topOfBucket4_ptr = thrust::raw_pointer_cast(d_topOfBucket4.data());
	int *d_numSift4_ptr = thrust::raw_pointer_cast(d_numSift4.data());

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust2 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==================  Call the GPU kernel  ====================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findTopOfBucket<<<numBlocks, threadsPerBlock>>>(d_gridID,d_gridID2,d_gridID3,d_gridID4, target.num_keys,d_topOfBucket_ptr,d_topOfBucket2_ptr,d_topOfBucket3_ptr,d_topOfBucket4_ptr,target_grid.numGrids,target_grid.numGridsXOv,target_grid.numGridsYOv,target_grid.numGridsXYOv);
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_topOfBucket = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	//========== Record the number of sifts in each grid ===========  

	threadsPerBlock = 512;
	numBlocks = ceil((float)(max(target_grid.numGrids,target_grid.numGridsXOv,target_grid.numGridsYOv,target_grid.numGridsXYOv))/threadsPerBlock);

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findNumSift<<<numBlocks, threadsPerBlock>>>(d_gridID,d_topOfBucket_ptr,d_gridID2,d_topOfBucket2_ptr,d_gridID3,d_topOfBucket3_ptr,d_gridID4,d_topOfBucket4_ptr,target_grid.numGrids,target_grid.numGridsXOv,target_grid.numGridsYOv,target_grid.numGridsXYOv,d_numSift_ptr,d_numSift2_ptr,d_numSift3_ptr,d_numSift4_ptr);
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_numSift= ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);


	//==============================================================


	//====================  CLUSTERING STEP ========================
	// This step deals with finding the epipolar lines for each sift 
	// of the source image and clustering sifts with similar epipolar
	// lines together.
	//==============================================================


	//=================== Find epipolar lines ======================
	// The kernel findEpipolarLine finds the epipolar line and stores 
	// stores its points of intersection with the boundaries of the  
	// target image in d_epipolarPoints.
	//==============================================================

	//=============== Decide kernel configuration ==================

	threadsPerBlock = 512;
	numBlocks = ceil(((float)source.num_keys)/threadsPerBlock);

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findEpipolarLine<<<numBlocks, threadsPerBlock>>>(d_fmatrix,d_keys1,source.width,source.height, target.width, target.height,d_epipolarPoints,source.num_keys);	
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_findEpiline = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==== Sort the epipolar lines (keys) with siftIDs as values ====

	clock_gettime(CLOCK_MONOTONIC, &t1);

	thrust::device_ptr<long long int> d_epipolarPoints_ptr(d_epipolarPoints);
	thrust::device_ptr<int> d_source_siftID_ptr(d_source_siftID);

	thrust::sort_by_key(d_epipolarPoints_ptr, d_epipolarPoints_ptr+source.num_keys,d_source_siftID_ptr);

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust4 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);


	//==============================================================

	// Current scene :
	// sifts with similar epipolar lines are together now
	// +----------------------+---+---+---+---+---+---+---+----
	// |Epipolar Line end pts | a | b | c | d | e | f | g | ... (sorted)
	// +----------------------+---+---+---+---+---+---+---+----
	// |SiftID                | m | n | o | p | q | r | s | ... (random order)
	// +----------------------+---+---+---+---+---+---+---+----
	// An array called cluster(h_cluster and d_cluster) will set the boundaries
	// If epl[i] - epl[i-1] < threshold then cluster[i] = 0 else cluster[i] = 1
	//==============================================================

	h_cluster[0] = 0;

	clock_gettime(CLOCK_MONOTONIC, &t1);

	cudaMemcpy(d_cluster, h_cluster, sizeof(int)*source.num_keys, cudaMemcpyHostToDevice); 

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_memcpy3 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//=== Cluster the epipolarPoints together (similar epipolar lines in one group) ===

	clock_gettime(CLOCK_MONOTONIC, &t1);

	clusterLines<<<numBlocks, threadsPerBlock>>>(d_epipolarPoints,d_cluster,source.num_keys);
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_clusterLines = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	// Scene after clustering :
	// sifts with similar epipolar lines are together now
	// +----------------------+---+---+---+---+---+---+---+----
	// |Epipolar Line end pts | a | b | c | d | e | f | g | ... (sorted)
	// +----------------------+---+---+---+---+---+---+---+----
	// |SiftID                | m | n | o | p | q | r | s | ... (random order)
	// +----------------------+---+---+---+---+---+---+---+----
	// |cluster               | 0 | 0 | 1 | 0 | 1 | 1 | 1 | ... 
	// +----------------------+---+---+---+---+---+---+---+----
	// Now compute prefix sum to assign cluster ids
	//==============================================================


	clock_gettime(CLOCK_MONOTONIC, &t1);

	thrust::device_ptr<int> d_cluster_ptr(d_cluster);

	thrust::inclusive_scan(d_cluster_ptr, d_cluster_ptr + source.num_keys, d_cluster_ptr);	

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust5 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//================= current scene example ======================
	//
	// +---------------+---+---+---+---+---+---+---+----
	// |id             | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 ...
	// +---------------+---+---+---+---+---+---+---+----
	// |siftID         | a | b | c | d | e | f | g | h ...
	// +---------------+---+---+---+---+---+---+---+----
	// |h_cluster      | 0 | 1 | 1 | 2 | 3 | 3 | 3 | 4 ...
	// +---------------+---+---+---+---+---+---+---+----
	//
	//  so we want cluster location to be like this
	//
	// +-----------------+---+---+---+---+---+---+---+----
	// |cluster_id       | 0 | 1 | 2 | 3 | 4 | ...
	// +-----------------+---+---+---+---+---+---+---+----
	// |cluster_location | 0 | 1 | 3 | 4 | 7 | ...
	// +-----------------+---+---+---+---+---+---+---+----
	//
	//==============================================================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	cudaMemcpy(h_cluster, d_cluster, sizeof(int)*source.num_keys, cudaMemcpyDeviceToHost); 

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_memcpy4 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================
	// =================  Record Number of Clusters ================

	int numClusters = h_cluster[source.num_keys-1]+1;

	//==============================================================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	thrust::device_vector<int> d_clusterLocation(numClusters, -1);
	d_clusterLocation[h_cluster[0]] = 0;

	int * d_clusterLocation_ptr = thrust::raw_pointer_cast(d_clusterLocation.data());

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust6 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	//==================== Find Cluster Location ===================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findClusterLocation<<<numBlocks, threadsPerBlock>>>(d_cluster, source.num_keys,d_clusterLocation_ptr,d_source_siftID,d_clusterID);
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_findClusterLocation = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	//===================== Matching Step ==========================
	//
	//    Matching involves the following two kernels : 
	//    -----------------------------------------------------------
	//
	// -> FindPotentialMatches which finds eqidistant points on the 
	//    epipolar line and the grids those points fall into. Then it
	//    stores the target sifts which are a part of those grids.
	//    These sifts are our potential matches. This is done for each 
	//    cluster.
	//
	//    -----------------------------------------------------------
	//
	// -> FindMatches which, for each source sift, loads the potential
	//    matches and computes the euclidian distances. It then takes 
	//    the ratio of minimum and next minimum distance and declares 
	//    a match based on some criterias.
	//	
	//==============================================================

// [date : 1 oct 2015] first we do a find potential matches to just find the num of potential matches for each cluster
// then we find the max of these num_potential_matches
// and then we malloc the d_pot_patches array with that size 

	//================= Decide kernel configuration ================

	threadsPerBlock = 256 ;
	numBlocks = numClusters;

	//==============================================================
	thrust::device_vector<int> d_numPotentialMatches(numClusters, 0);

	int * d_numPotentialMatches_ptr = thrust::raw_pointer_cast(d_numPotentialMatches.data());


	clock_gettime(CLOCK_MONOTONIC, &t1);

	findNumPotentialMatchesForEachCluster<<<numBlocks, threadsPerBlock, 2*threadsPerBlock*sizeof(int)>>>(d_gridID,d_target_siftID,d_topOfBucket_ptr,d_gridID2,d_target_siftID2,d_topOfBucket2_ptr,d_gridID3,d_target_siftID3,d_topOfBucket3_ptr,d_gridID4,d_target_siftID4,d_topOfBucket4_ptr,target_grid.gridSize,d_clusterLocation_ptr,d_epipolarPoints,d_numSift_ptr,d_numSift2_ptr,d_numSift3_ptr,d_numSift4_ptr,target.height, target_grid.numXGrids1,target_grid.halfSize,target_grid.numXGrids2,target_grid.numGridsXOv,target_grid.numGridsYOv,target_grid.numGrids,target_grid.numGridsXYOv,d_numPotentialMatches_ptr);	
	cudaThreadSynchronize();
	
//	for(int i = 0 ; i < d_numPotentialMatches.size(); i++)
//		std::cout << d_numPotentialMatches[i] << std::endl;

	thrust::device_vector<int>::iterator d_max = thrust::max_element(d_numPotentialMatches.begin(),d_numPotentialMatches.end());
//	int *result = thrust::min_element(d_numPotentialMatches.begin(), d_numPotentialMatches.end());
	int maxNumPotentialMatches = *d_max;

	printf("maxpot = %d\n",maxNumPotentialMatches);


	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_computeNumPotentialMatches = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);



	//================ Malloc d_potentialMatches ===================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	int *d_potentialMatches;
	cudaMalloc((void **)&d_potentialMatches,numClusters*(maxNumPotentialMatches+2)*sizeof(int));	// allocate memory on device

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_malloc4 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================



	//=========== Find Potential Matches for each cluster ==========

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findPotentialMatchesForEachCluster<<<numBlocks, threadsPerBlock, 2*threadsPerBlock*sizeof(int)>>>(d_gridID,d_target_siftID,d_topOfBucket_ptr,d_gridID2,d_target_siftID2,d_topOfBucket2_ptr,d_gridID3,d_target_siftID3,d_topOfBucket3_ptr,d_gridID4,d_target_siftID4,d_topOfBucket4_ptr,target_grid.gridSize,d_clusterLocation_ptr,d_epipolarPoints,d_numSift_ptr,d_numSift2_ptr,d_numSift3_ptr,d_numSift4_ptr,d_potentialMatches,target.height, target_grid.numXGrids1,target_grid.halfSize,target_grid.numXGrids2,target_grid.numGridsXOv,target_grid.numGridsYOv,target_grid.numGrids,target_grid.numGridsXYOv,maxNumPotentialMatches);	
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_computePotentialMatches = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	// Declare matches array which stores the matching siftID 
	// ( or -1 if no match found) for each query sift 

	clock_gettime(CLOCK_MONOTONIC, &t1);

	int *match = (int *)malloc(sizeof(int)*source.num_keys);
	int *d_matches_ptr;
	cudaMalloc((void**)&d_matches_ptr, sizeof(int)*source.num_keys);
	for(i = 0 ; i < source.num_keys; i++)
	{
		match[i] = -1;
	}
	cudaMemcpy(d_matches_ptr, match, sizeof(int)*source.num_keys, cudaMemcpyHostToDevice); 

	int *h_potentialMatches = (int *)malloc(numClusters*(2+maxNumPotentialMatches)*sizeof(int));	// allocate memory on device
	cudaMemcpy(h_potentialMatches , d_potentialMatches,numClusters*(maxNumPotentialMatches+2)*sizeof(int) , cudaMemcpyDeviceToHost); 
	/*
	for(int i = 0 ; i < numClusters; i++)
	{
		if(h_potentialMatches[maxNumPotentialMatches*i] != 0)
			printf("numpotm = %d\n",h_potentialMatches[maxNumPotentialMatches*i]);
	}
*/

//	thrust::device_vector<int> d_matches(source.num_keys, -1);
//	int * d_matches_ptr = thrust::raw_pointer_cast(d_matches.data());

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_thrust3 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	//================= Decide kernel configuration ================

	threadsPerBlock = 256 ;
	numBlocks = source.num_keys;

	//==============================================================

	//======================== Find Matches ========================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	findMatches<<<numBlocks, threadsPerBlock,4*threadsPerBlock*sizeof(int)>>>(d_matches_ptr,d_clusterID,d_potentialMatches,maxNumPotentialMatches);
	cudaThreadSynchronize();

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_computeMatches = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);
	//==============================================================

	cudaMemcpy(match, d_matches_ptr, sizeof(int)*source.num_keys, cudaMemcpyDeviceToHost); 
	//======================  memcpy  ==============================

	clock_gettime(CLOCK_MONOTONIC, &t1);

//	thrust::host_vector<int> h_matches = d_matches;

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_memcpy5 = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);


	//======================== CLEAN UP ============================

	//===================== bind the textures ====================

	clock_gettime(CLOCK_MONOTONIC, &t1);

	cudaUnbindTexture (tex_targetDescriptor);
	cudaUnbindTexture (tex_sourceDescriptor);

	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_unbindtex = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	//==============================================================

	//===================== Free device memory =====================

	clock_gettime(CLOCK_MONOTONIC, &t1);

//	cudaFree(d_keys1);
//	cudaFree(d_keys2);

//	cudaFree(d_source_x);
//	cudaFree(d_source_y);

//	cudaFree(d_target_x);
//	cudaFree(d_target_y);

////	cudaFree(d_source_keypoints);
////	cudaFree(d_target_keypoints);

	cudaFree(d_source_siftID);
	cudaFree(d_target_siftID);
	cudaFree(d_target_siftID1);
	cudaFree(d_target_siftID2);
	cudaFree(d_target_siftID3);
	cudaFree(d_target_siftID4);

	cudaFree(d_fmatrix);

	cudaFree(d_gridID);
	cudaFree(d_gridID1);
	cudaFree(d_gridID2);
	cudaFree(d_gridID3);
	cudaFree(d_gridID4);

	cudaFree(d_epipolarPoints);
	cudaFree(d_cluster);
	cudaFree(d_clusterID);

	cudaFree(d_potentialMatches);
	cudaFree(d_matches_ptr);
	//======================== Free host memory ====================

/*	free(h_target_siftID);
	free(h_gridID);
	free(target_x);
	free(target_y);
	free(target_scale);
	free(target_orient);
	free(target_keypoints);
	free(h_source_siftID);
	free(source_x);
	free(source_y);
	free(source_scale);
	free(source_orient);
	free(source_keypoints);
*/	free(h_cluster);
	free(h_epipolarPoints);
	free(h_gridID);
	free(h_potentialMatches);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	double time_freemem = ((t2.tv_sec - t1.tv_sec)*1000) + (((double)(t2.tv_nsec - t1.tv_nsec))/1000000.0);

	clock_gettime(CLOCK_MONOTONIC, &t4);
	double time_total = ((t4.tv_sec - t3.tv_sec)*1000) + (((double)(t4.tv_nsec - t3.tv_nsec))/1000000.0);
	double time_gpu = ((t4.tv_sec - t5.tv_sec)*1000) + (((double)(t4.tv_nsec - t5.tv_nsec))/1000000.0);
	//==============================================================

	int num_matches = 0;

	for(i = 0 ; i < source.num_keys; i++)
	{
		if(match[i]!=-1)
		{
			num_matches++;
			matches.push_back(std::make_pair(i,match[i]));
		}
	}
	free(match);

	//==============================================================
  /*
	std::cout << "Time on GPU init: " << time_gpu_init << std::endl;	
	std::cout << "Time on CPU setup: " << time_total_cpu_alloc << std::endl;	
	std::cout << "Time - memcpy : " << time_memcpy1+time_memcpy3+time_memcpy4+time_memcpy5 << std::endl;
	printf("time_memcpy1 %lf, time_memcpy3 %lf , time_memcpy4 %lf, time_memcpy5 %lf\n",time_memcpy1,time_memcpy3,time_memcpy4,time_memcpy5);
	std::cout << "Time - malloc : " << time_malloc1+time_malloc4 << std::endl;
	std::cout << "Time to compute gridID : " << time_computeGridID << std::endl;
	std::cout << "Time - thrust : " << time_thrust1+time_thrust2+time_thrust3+time_thrust4+time_thrust5+time_thrust6 << std::endl;
	std::cout << "Time to compute topOfBucket : " << time_topOfBucket << std::endl;
	std::cout << "Time to find num sifts per grid : " << time_numSift << std::endl;
	std::cout << "Time to find epipolar lines : " << time_findEpiline << std::endl;
	std::cout << "Time to cluster lines : " << time_clusterLines << std::endl;
	std::cout << "Time to find cluster location : " << time_findClusterLocation << std::endl;
	
	std::cout << "Time to compute num potential matches per cluster: " << time_computeNumPotentialMatches << std::endl;
	std::cout << "Time to compute potential matches per cluster: " << time_computePotentialMatches << std::endl;
	std::cout << "Time to compute matches: " << time_computeMatches << std::endl;
	std::cout << "Time to unbind textures : " << time_unbindtex << std::endl;
	std::cout << "Time to free memory : " << time_freemem << std::endl;
	std::cout << "total time on gpu: " << time_gpu << std::endl;
	std::cout << "total time : " << time_total << std::endl;
*/
	std::cout << "Total matches found : " << num_matches << std::endl;

	std::cout << time_gpu << std::endl;
	return time_gpu ;




	//==============================================================

}
