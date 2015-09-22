/* VLFeat_SIFT.cpp : Defines the entry point for the console application.

Integration of the classifier Copyright (c) 2014
Wilfried Hartmann and Michal Havlena, IGP, D-BAUG, ETH Zurich.

When using the code either in source or binary form, please cite:
"W.Hartmann, M.Havlena, and K.Schindler: Predicting Matchability, CVPR 2014"
http://www.igp.ethz.ch/photogrammetry/publications/pdf_folder/CVPR2014_Hartmann.pdf

Random forest classifier code Copyright (c) 2014 Stefan Walk.
Code made available under the terms of the MIT license (see the LICENSE file).
*/

/** @internal
 ** @file     sift.c
 ** @author   Andrea Vedaldi
 ** @brief    Scale Invariant Feature Transform (SIFT) - Driver
 **/

/*
Copyright (C) 2007-12 Andrea Vedaldi and Brian Fulkerson.
All rights reserved.

This file is part of the VLFeat library and is made available under
the terms of the BSD license (see the COPYING file).
*/

extern "C" {
//#include <vl/generic.h>
//#include <vl/getopt_long.h>
}

#include <cstddef>
#include "random-forest/node-gini.hpp"
#include "random-forest/forest.hpp"

#include <boost/archive/text_iarchive.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fstream>
#include <sstream>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <vector>
#include "keys2a.h"

using namespace std;

int main(int argc, char **argv)
{
	printf("\nLiblearning");
	fflush(stdout);
	liblearning::RandomForest::RandomForest< liblearning::RandomForest::NodeGini<liblearning::RandomForest::AxisAlignedSplitter> > rtrees ;


	std::ifstream ifs("rforest.gz", std::ios_base::in | std::ios_base::binary) ;
	
  	char     err_msg [1024] ;
	if (!ifs.is_open()) {
		snprintf(err_msg, sizeof(err_msg),
				"Could not open rforests for reading.") ;
		fprintf (stderr, "sift: err: %s\n", err_msg) ;
		return 1 ;
	}
	
	printf("sift: read classifier from 'rforests'\n") ;
	
	boost::iostreams::filtering_istream ins ;
	ins.push(boost::iostreams::gzip_decompressor()) ;
	ins.push(ifs) ;
	boost::archive::text_iarchive ias(ins) ;
	ias >> BOOST_SERIALIZATION_NVP(rtrees) ;


	unsigned char* desc;
	keypt_t* keys;
	int numKeys = ReadKeyFile(argv[1], &desc, &keys);

	printf("\nRead %d keys", numKeys);

	float cl_thresh = 0.525;


	printf("\nLiblearning");
	fflush(stdout);
	for(int i=0; i < numKeys; i++) {
		//printf("\nKey %d", i);
		fflush(stdout);
		std::vector<float> siftdesc(128) ;
		std::vector<float> results(rtrees.params.n_classes);

		unsigned char* des = desc+128*i;
		for (int l = 0; l < 128; ++l)
			siftdesc [l] = (float)(des[l]) ;

		//printf("\nKey %d copied", i);
		fflush(stdout);
		rtrees.evaluate(&siftdesc[0], &results[0]) ;

		//printf("\nKey %d evaluated", i);
		fflush(stdout);

		if (results[0] > 1.0 - cl_thresh) {
			continue ; 
		} else {
			printf("%d\n", i);
		}
	}

	/* quit */
	return 0 ;
}
