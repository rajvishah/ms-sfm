/*===========================================================================*\
 *                                                                           *
 *                            ACG Localizer                                  *
 *      Copyright (C) 2012 by Computer Graphics Group, RWTH Aachen           *
 *                           www.rwth-graphics.de                            *
 *                                                                           *
 *---------------------------------------------------------------------------* 
 *  This file is part of ACG Localizer                                       *
 *                                                                           *
 *  ACG Localizer is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation, either version 3 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  ACG Localizer is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with ACG Localizer.  If not, see <http://www.gnu.org/licenses/>.   *
 *                                                                           *
\*===========================================================================*/ 


#define __STDC_LIMIT_MACROS

// C++ includes
#include <vector>
#include <list>
#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <climits>
#include <float.h>
#include <cmath>
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <queue>

// includes for classes dealing with SIFT-features
#include "features/SIFT_loader.hh"
#include "features/visual_words_handler.hh"

// stopwatch
#include "timer.hh"

// math functionality
#include "math/projmatrix.hh"
#include "math/matrix3x3.hh"

// tools to parse a bundler reconstruction
#include "sfm/parse_bundler.hh"
#include "sfm/bundler_camera.hh"

// RANSAC
#include "RANSAC.hh"

// exif reader to get the width and height out 
// of the exif tags of an image
#include "exif_reader/exif_reader.hh"

// ANN Libary, used to perform search in 3D
#include "ANN/ANN.h"

// simple vector class for 3D points
#include <OpenMesh/Core/Geometry/VectorT.hh>

int main (int argc, char **argv)
{
 
    clock_t global_start = clock();
    std::vector< std::string > key_filenames;
    std::map<int,int> queryKeyMap;
    std::string keylist = std::string(argv[1]) + "/queries.txt";
    key_filenames.clear();
    {
        std::ifstream ifs( keylist.c_str(), std::ios::in );
        std::string tmp_string;
        int i=0;
        while(std::getline(ifs, tmp_string))
        {
            if( !tmp_string.empty() )
            {
                std::cout << tmp_string << std::endl;
                std::istringstream strm(tmp_string);
                int id;
                strm >> id;
                std::string queryKeyName;
                strm >> queryKeyName;
                std::printf("\nId %d, File %s", id, queryKeyName.c_str());
                std::fflush(stdout);
                key_filenames.push_back(queryKeyName.c_str());
                queryKeyMap.insert(std::make_pair(i, id));
                i++;
            }
        }
        ifs.close();
        std::cout << "Here" << std::endl;
        std::cout << " done loading " << key_filenames.size() << " keyfile names " << std::endl;
    }
    
    std::string matchesInPath = std::string(argv[1]) + "/correspondences/";
    int queryCount = std::atoi(argv[2]); 
    int queryToLocalize = queryKeyMap[queryCount];

    printf("\nPicking matches file %d for query %d", queryToLocalize, queryCount);
    char matchFileName[1024];
    sprintf(matchFileName,"%s/matches_3D-2D_img-%04d.txt", matchesInPath.c_str(), queryToLocalize);
    std::ifstream ifsMatch( matchFileName, std::ios::in );
    if( !ifsMatch.is_open() )
    {
        std::cerr << " Could not open matches file " << matchFileName << std::endl;
        return -1;
    }
  
    std::vector< float > c2D, c3D;
    c2D.clear();
    c3D.clear();


    FILE *corresFile = fopen(matchFileName, "r");
    if(corresFile == NULL) {
        printf("\nError reading input file");
        return -1;
    }

    std::vector< std::pair<int, int> > finalCorrespondences;
    int pt3idx, pt2idx;
    float x3d, y3d, z3d, x2d, y2d;
    int nb_corr = 0;
    while(fscanf(corresFile,"%d %d %f %f %f %f %f", &pt2idx, &pt3idx, &x2d, &y2d, &x3d, &y3d, &z3d) != EOF) {
        c3D.push_back(x3d);
        c3D.push_back(y3d);
        c3D.push_back(z3d);
        c2D.push_back(x2d);
        c2D.push_back(y2d);

        finalCorrespondences.push_back(std::make_pair(pt2idx, pt3idx));
        nb_corr++;
    }
      
    clock_t local_start = clock();
    double ransac_max_time = 60.0; 
    uint32_t minimal_RANSAC_solution = 12;
    float min_inlier = 0.2f;
    double RANSAC_time = 0.0;
    RANSAC::computation_type = P6pt;
    RANSAC::stop_after_n_secs = true;
    RANSAC::max_time = ransac_max_time;
    RANSAC::error = 10.0f; // for P6pt this is the SQUARED reprojection error in pixels
    RANSAC ransac_solver;
     
    ransac_solver.apply_RANSAC( c2D, c3D, nb_corr, std::max( float( minimal_RANSAC_solution ) / float( nb_corr ), min_inlier ) ); 
    
    
    // output the solution:
    std::cout << "#### found solution ####" << std::endl;
//    std::cout << " needed time: " << all_timer.GetElapsedTimeAsString() << std::endl;
    
    // get the solution from RANSAC
    std::vector< uint32_t > inlier;
    
    inlier.assign( ransac_solver.get_inliers().begin(), ransac_solver.get_inliers().end()  );
    
    Util::Math::ProjMatrix proj_matrix = ransac_solver.get_projection_matrix();
    Util::Math::ProjMatrix proj_matrix2 = ransac_solver.get_projection_matrix();
    
    // decompose the projection matrix
    Util::Math::Matrix3x3 Rot, K;
    proj_matrix.decompose( K, Rot );
    proj_matrix.computeInverse();
    proj_matrix.computeCenter();
    std::cout << " camera calibration: " << K << std::endl;
    std::cout << " camera rotation: " << Rot << std::endl;
    std::cout << " camera position: " << proj_matrix.m_center << std::endl;

    
    // write details to the details file. per line:
    // #inlier #correspondences time for vw time for corr time for ransac
    std::string outPath = std::string(argv[1]) + "/CamParFiles/"; 
    std::string outFile;
    char timeFileName[1000];
    sprintf(timeFileName,"%s/timing-%04d.txt", outPath.c_str(), queryToLocalize);
    outFile=std::string(timeFileName);
    std::ofstream ofs( outFile.c_str(), std::ios::out );

    if( !ofs.is_open() )
    {
        std::cerr << " Could not write results to " << outFile << std::endl;
        return 1;
    }

    char parFileName[1024];
    sprintf(parFileName,"%s/%04d-cam-par.txt", outPath.c_str(), queryToLocalize);
    std::ofstream ofsPar( parFileName, std::ios::out );

    if( !ofsPar.is_open() )
    {
        std::cerr << " Could not write results to " << parFileName << std::endl;
        return 1;
    }

    ofs << inlier.size() << " " << nb_corr << " " << RANSAC_time << " " << std::endl;
    
    std::cout << "#########################" << std::endl;
    
    ////
    // Update statistics
    
    if( inlier.size() >= minimal_RANSAC_solution )
    {
        ofsPar << inlier.size() << std::endl;
        for(int inl=0; inl < inlier.size(); inl++) {
            std::pair<int,int> pair = finalCorrespondences[inlier[inl]];
            ofsPar << pair.first << " " << pair.second << std::endl;
        }

        ofsPar << proj_matrix2 << std::endl;
        ofsPar << K << std::endl;
        ofsPar << Rot << std::endl;
        ofsPar << proj_matrix.m_center << std::endl;
    }
    ofsPar.close();
    

  clock_t local_end = clock();
  printf("[ACG_LOCALIZER] Overall took %0.3fs\n", 
          (local_end - global_start) / ((double) CLOCKS_PER_SEC));
  printf("[ACG_LOCALIZER] Localization took %0.3fs\n", 
          (local_end - local_start) / ((double) CLOCKS_PER_SEC));
  
 
  return 0;
}

