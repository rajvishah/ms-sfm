#include "defs.h"
#include "sifts.h"
class Matcher
{
	public :
		int num_matches;

		std::vector <std::pair<int, int> > matches; 

		double matchImagePair(sift_img source,sift_img target,float *f_matrix,SiftGPU::SiftKeypoint* &d_keys1,unsigned char* &descriptors1 ,SiftGPU::SiftKeypoint* &d_keys2,unsigned char* &descriptors2 );
			void visualizeMatches(sift_img source,sift_img target,std::vector<std::pair<int,int> > matches,std::vector<SiftGPU::SiftKeypoint> &keys1,std::vector<SiftGPU::SiftKeypoint> &keys2) ;
};
