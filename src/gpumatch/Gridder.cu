#include "defs.h"
#include "Gridder.h"
#include "sifts.h"

void Grid::g_init(sift_img h_query)
{

	int num_keys = h_query.num_keys;
	gridSize = 8;	
	imageWidth = h_query.width;
	imageHeight = h_query.height;

	if(gridSize%2 == 1) gridSize++;
	
	halfSize = gridSize/2;

	numXGrids1 = (int)ceil((float)imageWidth/gridSize);
	numYGrids1 = (int)ceil((float)imageHeight/gridSize);
	numXGrids2 = (int)ceil((float)(imageWidth-halfSize)/gridSize);
	numYGrids2 = (int)ceil((float)(imageHeight-halfSize)/gridSize);

	numGrids = numXGrids1*numYGrids1;
	numGridsXOv = numXGrids2*numYGrids1;
	numGridsYOv = numXGrids1*numYGrids2;
	numGridsXYOv = numXGrids2*numYGrids2;
	//	int numGridsXOv = numXGrids2*numYGrids1;
	//	int numGridsYOv = numXGrids1*numYGrids2;
	//	int numGridsXYOv = numXGrids2*numYGrids2;

	ov = (float)halfSize/(float)gridSize;

//==============================================================


}
