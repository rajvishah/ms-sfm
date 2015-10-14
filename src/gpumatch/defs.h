#include <stdio.h>
#include <stdlib.h>

#include <cuda.h>

#include <cmath>
#include <time.h>

#include <string>
#include <vector>
#include <map>


#include <fstream>
#include <iostream>
#include <jpeglib.h>


#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include <opencv/cvaux.h>
#include <opencv/cxcore.h>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/device_ptr.h>
#include <thrust/fill.h>
#include <thrust/sort.h>
#include <thrust/transform.h>
#include <thrust/functional.h>
#include <thrust/execution_policy.h>
#include <thrust/system/system_error.h>

#include "/home/vanshika/SiftGPU/src/SiftGPU/SiftGPU.h"


#define MAX_TO_MATCH 4096
