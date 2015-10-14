#include "defs.h"
#include "sifts.h"
#pragma once
class Grid
{
	public:
		int gridSize ;	
		int imageWidth;
		int imageHeight;
		int halfSize;
		int numXGrids1;
		int numYGrids1;
		int numXGrids2;
		int numYGrids2;

		int numGrids;
		int numGridsXOv;
		int numGridsYOv;
		int numGridsXYOv;
		float ov;

		void g_init(sift_img h_query);
};
