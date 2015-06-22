#ifndef __IMAGESIZE_H
#define __IMAGESIZE_H
#include "defs.h"
#include "jpeglib.h"
class ImageSize {
	public:
		static bool GetSize(string &filename, int* width, int* height);
};

 
#endif //__IMAGESIZE_H 
