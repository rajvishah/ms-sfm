/*===========================================================================*\
 *                                                                           *
 *                            ACG Localizer                                  *
 *      Copyright (C) 2011-2012 by Computer Graphics Group, RWTH Aachen      *
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

#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <cmath>
#include <algorithm>
#include <float.h>
#include <stdio.h>
#include <map>
#include <stdlib.h>

#include "sfm/parse_bundler.hh"

#include "features/visual_words_handler.hh"



////
// functions used inside the main function
////

// computes medoid descriptor from a vector of descriptors. returns the index of the medoids
uint32_t compute_medoid( std::vector< unsigned char > &desc )
{
  uint32_t med_id = 0;
  
  uint32_t nb_desc = uint32_t( desc.size() );
  
  double max_dist = DBL_MAX;
  
  for( uint32_t i=0; i<nb_desc; i+=128 )
  {
    double cur_dist = 0.0;
    for( uint32_t j=0; j<nb_desc; j+=128 )
    {
      double dist = 0.0;
      for( uint32_t k=0; k<128; ++k )
      {
        double x = desc[i+k] - desc[j+k];
        dist += x*x;
      }
      cur_dist += sqrt(dist);
    }
    if( cur_dist < max_dist )
    {
      max_dist = cur_dist;
      med_id = i/128;
    }
  }
  
  return med_id;
}

// computes the mean descriptor from a vectors of descriptors. the mean is stored in the variable mean
void compute_mean( std::vector< unsigned char > &desc, std::vector< float > &mean )
{
  mean.resize( 128, 0 );
  uint32_t N = (uint32_t) desc.size() / 128;
  for( uint32_t i=0; i<N; ++i )
  {
    uint32_t index = i*128;
    for( uint32_t j=0; j<128; ++j )
      mean[j] += (float) desc[index+j];
  }
  float n = float(N);
  for( int j=0; j<128; ++j )
    mean[j] /= n;
}


int main (int argc, char **argv)
{
 
  if( argc < 9 )
  {
    std::cout << "_______________________________________________________________________________________________________" << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -    Program to compute the assignments of descriptors and 3D points to visual words.               - " << std::endl;
    std::cout << " -                               2012 by Torsten Sattler (tsattler@cs.rwth-aachen.de)                - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " - usage: compute_desc_assignments bundle nb_trees nb_cluster cluster out_desc mode assignment_type  - " << std::endl;
    std::cout << " - Parameters:                                                                                       - " << std::endl;
    std::cout << " -  bundle                                                                                           - " << std::endl;
    std::cout << " -     Filename of a Bundler info file as generated by Bundle2Info.                                  - " << std::endl; 
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  nb_trees                                                                                         - " << std::endl;
    std::cout << " -     The number of trees to use. (Set to 1 for all experiments from used in the paper.)            - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  nb_cluster                                                                                       - " << std::endl;
    std::cout << " -     The number of visual words in the file.                                                       - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  clusters                                                                                         - " << std::endl;
    std::cout << " -     The cluster centers (visual words), stored in a textfile consisting of                        - " << std::endl;
	std::cout << " -     nb_clusters * 128 floating point values.                                                      - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  out_desc                                                                                         - " << std::endl;
    std::cout << " -     The file the computed descriptor-to-visual word assignments should be saved into.             - " << std::endl;
    std::cout << " -     The output file is a binary file of the following format:                                     - " << std::endl;
    std::cout << " -     #points #cluster #non-empty cluster #descriptors (all uint32_t)                               - " << std::endl;
    std::cout << " -     The 3D points (in total #points * 3 floats).                                                  - " << std::endl;
    std::cout << " -     The descriptors (in total #descriptors * 128 unsigned chars / floats). (depending on mode)    - " << std::endl;
    std::cout << " -     The assignments. For every non-empty visual word:                                             - " << std::endl;
    std::cout << " -       cluster-id #assignments assignments (every assignment is a pair of uint32_t) (all uint32_t) - " << std::endl;
    std::cout << " -     If the filename of out_desc is \"analyze_assignments\" (without quotation marks) no output is - " << std::endl;
    std::cout << " -     generated and certain tests about the assignments are performed.                              - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  mode                                                                                             - " << std::endl;
    std::cout << " -     0 - Medoid per visual word: Assign each descriptor to its visual word, use the medoid to      - " << std::endl;
    std::cout << " -         represent descriptors belonging to the same 3D point that are assigned to the same visual - " << std::endl;
    std::cout << " -         word. The descriptors are stored using unsigned char values.                              - " << std::endl;
    std::cout << " -     1 - For each 3D point, compute the mean descriptor (float) and assign it to its visual word.  - " << std::endl;
    std::cout << " -     2 - Mean per point: For each 3D point, compute the mean descriptor (float) and assign it to   - " << std::endl;
    std::cout << " -         every visual word to which at least one of the descriptors of the 3D point would be       - " << std::endl;
	std::cout << " -         assigned.                                                                                 - " << std::endl;
    std::cout << " -     3 - Medoid per point: For each 3D point, compute the medoid descriptor (unsigned char) and    - " << std::endl;
	std::cout << "           assign it to every visual word to which at least one of the descriptors of the 3D point   - " << std::endl;
	std::cout << "           would be assigned.                                                                        - " << std::endl;
    std::cout << " -     4 - Mean per visual word: Similar to 0 only using the mean instead of the medoid.             - " << std::endl;
    std::cout << " -     5 - All descriptors: Use all descriptors, stored as unsigned chars.                           - " << std::endl;
    std::cout << " -     6 - Integer mean per visual word: Use integer means for every point and visual word. Similar  - " << std::endl;
    std::cout << " -         to 4, but mean descriptor is rounded to the next integer and saved as an unsigned char.   - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  assignment_type                                                                                  - " << std::endl;
    std::cout << " -     Set to 0 to use a single kd-tree, set to 1 to use a vocabulary tree (hkmeans tree) to         - " << std::endl;
    std::cout << " -     compute the assignments of the point descriptors to visual words.                             - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << " -  bundle_type                                                                                      - " << std::endl;
    std::cout << " -     The file format of the binary .info file (parameter bundle). If you used the Bundle2Info      - " << std::endl;
    std::cout << " -     program to generate the info file, set this parameter to 0. If you are using the aachen.info  - " << std::endl;
    std::cout << " -     file from the Aachen dataset (available on the website), then you have to set this parameter  - " << std::endl;
    std::cout << " -     to 1 since file also contains camera information.                                             - " << std::endl;
    std::cout << " -                                                                                                   - " << std::endl;
    std::cout << "_______________________________________________________________________________________________________" << std::endl;
    return -1;
  }
  
  ////
  // get the parameters
  std::string bundle( argv[1] );
  size_t nb_trees = (size_t) atoi( argv[2] );
  uint32_t nb_cluster = atoi( argv[3] );
  std::string cluster_file( argv[4] );
  std::string desc_output( argv[5] );
  int mode = atoi( argv[6] );
  if( mode < 0 || mode > 6 )
  {
    std::cerr << " ERROR: Unknown mode " << mode << ", aborting " << std::endl;
    return -1;
  }
  
  int assignment_type = atoi( argv[7] );
  if( assignment_type < 0 || assignment_type > 1 )
  {
    std::cerr<< " ERROR: Unknown type to compute the visual word assignments." << std::endl;
    return -1;
  }
  
  int bundle_type = atoi( argv[8] );
  if( bundle_type < 0 || bundle_type > 1 )
  {
    std::cerr<< " ERROR: Unknown file format for the binary .info file." << std::endl;
    return -1;
  }
 
  clock_t start1 = clock();
  ////
  // load the Bundler data
  std::cout << "-> parsing the bundler output from " << bundle << std::endl;
  parse_bundler parser;
  parser.load_from_binary( bundle.c_str(), bundle_type );
  std::vector< feature_3D_info >& feature_infos = parser.get_feature_infos();
  
  uint32_t nb_points = feature_infos.size();
  std::cout << "--> done parsing the bundler output " << std::endl;
 clock_t end1 = clock();
 
  ////
  // load the trees for assigning visual words
  visual_words_handler vw_handler;
  vw_handler.set_nb_trees( nb_trees );
  vw_handler.set_nb_visual_words( nb_cluster );
  vw_handler.set_branching( 10 );
  
  vw_handler.set_method(std::string("flann"));
  if( assignment_type == 0 )
    vw_handler.set_flann_type(std::string("randomkd"));
  else
    vw_handler.set_flann_type(std::string("hkmeans"));
    
  if( !vw_handler.create_flann_search_index( cluster_file ) )
  {
	std::cout << " ERROR: Could not load the cluster centers from " << cluster_file << std::endl;;
	return -1;
  } 
  
 
  clock_t start2 = clock();
  ////
  // Compute the assignments to visual words for all descriptors.
  // To do so, we go through all 3D points and assign their descriptors 
  // to the visual words. To keep track of the assignments, we use one 
  // array that stores for every assigned descriptor the id of the visual word
  // it was assigned to.
  std::cout << "-> Assigning the descriptors to visual words (this might take a while)" << std::endl;
  
  uint32_t total_nb_descriptors = 0;
  for( uint32_t i=0; i<nb_points; ++i )
    total_nb_descriptors += (uint32_t) feature_infos[i].view_list.size();
  
  // we do the assignments in batches of 10000 descriptors to speed things up
  uint32_t batch_size = 10000;
  
  uint32_t *descriptor_2_vw_assignments = new uint32_t[ total_nb_descriptors ];
  uint32_t offset = 0;
  uint32_t selected_desc = 0;
  uint64_t index1, index2;
  std::vector< unsigned char > tmp_descriptors( 128 * batch_size );
  
  std::vector< uint32_t > cluster_assignments_( batch_size, 0 );
  
  // execute the batches
  uint32_t total_offset = 0;
  for( uint32_t i=0; i<nb_points; ++i )
  {
    uint32_t nb_desc_i = (uint32_t) feature_infos[i].view_list.size();
    
    total_offset += nb_desc_i;
    
    for( uint32_t j=0; j<nb_desc_i; ++j )
    {
      // copy the descriptor
      index1 = selected_desc * 128;
      index2 = uint64_t(j)*uint64_t(128);
      for( uint64_t k=0; k<128; ++k )
		tmp_descriptors[index1+k] = feature_infos[i].descriptors[index2+k];
	  
      ++selected_desc;
	  
	  // check if we have selected enough descriptors to do a batch assignment
      if( (selected_desc == batch_size) || (offset+selected_desc == total_nb_descriptors) || (total_offset+selected_desc == total_nb_descriptors ) )
      {
        if( assignment_type == 0 )
          vw_handler.set_nb_paths( 10 );
        else
          vw_handler.set_nb_paths( 1 );
        
		vw_handler.assign_visual_words_uchar( tmp_descriptors, selected_desc, cluster_assignments_ );
	
		for( uint32_t l=0; l<selected_desc; ++l )
		{
		  if( cluster_assignments_[l] > nb_cluster )
		  {
			std::cout << cluster_assignments_[l] << " : ";
			for( uint32_t ll=0; ll<128; ++ll )
			  std::cout << " " << int(tmp_descriptors[128*l+ll]);
			std::cout << std::endl;
		  }
		  descriptor_2_vw_assignments[offset+l] = cluster_assignments_[l];
		}
		
// 		std::cout << offset << " / " << total_nb_descriptors << std::endl;
	
	
		offset += selected_desc;
		selected_desc = 0;
      }
    } 
  }
  std::cout << "--> done" << std::endl;  

  clock_t end2 = clock();


  ////
  // Now compute the representatives for the 3D points
 
  clock_t start3 = clock();
  std::cout << "-> Computing the representative descriptors for every 3D point" << std::endl;
  
  
  // vectors containing all descriptors (simply concatenating all descriptor entries)
  // depending on the mode, we use either unsigned char values or float values to represent
  // descriptor entries
  std::vector< unsigned char > descriptors;
  descriptors.clear();
  
  std::vector< float > descriptors_float;
  descriptors_float.clear();
  
  // vector containing for every visual word a list of (point id, descriptor id) pairs, where 
  // point id is the index of the point in feature_infos and 128 * descriptor id is the first entry
  // of the descriptor belonging to that point in either descriptors or descriptors_float (depending on the mode)
  std::vector< std::vector< std::pair< uint32_t, uint32_t > > > vw_point_descriptor_idx( nb_cluster );
  
  for( uint32_t i=0; i<nb_cluster; ++i )
    vw_point_descriptor_idx[i].clear();
  
  
  offset = 0;
  

  for( uint32_t i=0; i<nb_points; ++i )
  { 
	// get the number of views of that point, which coincides with the number of 
	// descriptors available for that point
    uint32_t nb_desc_i = (uint32_t) feature_infos[i].view_list.size();  
    
    // compute representatives depending on the mode chosen by the user
    if ( mode == 0 )
    {
      // compute for each visual word the medoid descriptors and store it
      // first determine the number of activated vw
      std::set< size_t > activated_visual_words;
      activated_visual_words.clear();
      for( size_t j=0; j<nb_desc_i; ++j )
		activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
      
      // now we compute the medoid descriptors
      for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
      {
		// gather all descriptors assigned to visual word *it
		std::vector< unsigned char > visual_word_descriptors;
		visual_word_descriptors.clear();
		for( size_t j=0; j<nb_desc_i; ++j )
		{
		  if( *it == descriptor_2_vw_assignments[offset+j] )
		  {
			for( uint32_t k=uint32_t(j)*128; k < uint32_t(j)*128+128; ++k )
			  visual_word_descriptors.push_back(feature_infos[i].descriptors[k]);
		  }
		}
		
		// now compute the medoid
		uint32_t med_id = compute_medoid( visual_word_descriptors );
		
		// get the id of the new medoid descriptor for the (point id, descriptor id) pair
		uint32_t desc_id = uint32_t( descriptors.size() ) / 128;
		
		// add the new descriptor to the set of all used descriptors
		for( uint32_t k=128*med_id; k<(128*med_id + 128 ); ++k )
		  descriptors.push_back(visual_word_descriptors[k]);

		// insert the pair
		vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );

      }
    }
    else if( mode == 3 )
    {
      // compute the medoid descriptor for the 3D point and assign it to all visual words that one of the 
      // descriptors is assigned to
      
      // first determine the number of unique activated vw
      std::set< size_t > activated_visual_words;
      activated_visual_words.clear();
      for( size_t j=0; j<nb_desc_i; ++j )
		activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
      
      // compute the medoid
      uint32_t med_id = compute_medoid( feature_infos[i].descriptors );
      uint32_t desc_id = uint32_t( descriptors.size() ) / 128;
      
      // insert the medoid and add references to it
      for( uint32_t k=128*med_id; k<(128*med_id + 128 ); ++k )
		descriptors.push_back(feature_infos[i].descriptors[k]);
      
      for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
		vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );
    }
    else if( mode == 4 )
    {
      // compute for each visual word the mean descriptors and assign it to all visual words that one of the 
      // descriptors is assigned to
	  
      // first determine the number of activated vw
      std::set< size_t > activated_visual_words;
      activated_visual_words.clear();
      for( size_t j=0; j<nb_desc_i; ++j )
		activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
      
      // now we compute the mean descriptors belonging to the visual words
      for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
      {
		// gather all descriptors assigned to visual word *it
		std::vector< unsigned char > visual_word_descriptors;
		visual_word_descriptors.clear();
		for( size_t j=0; j<nb_desc_i; ++j )
		{
		  if( *it == descriptor_2_vw_assignments[offset+j] )
		  {
			for( uint32_t k=uint32_t(j)*128; k < uint32_t(j)*128+128; ++k )
			  visual_word_descriptors.push_back(feature_infos[i].descriptors[k]);
		  }
		}
		
		// now compute the mean
		std::vector< float > mean_descriptor;
		
		compute_mean( visual_word_descriptors, mean_descriptor );
		
		uint32_t desc_id = uint64_t( descriptors_float.size() ) / 128;
		
		// store the descriptor
		for( uint32_t k=0; k<128; ++k )
		  descriptors_float.push_back(mean_descriptor[k]);

		vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );
		
		mean_descriptor.clear();
      }
    }
    else if( mode == 5 )
    {
	  // all descriptors
	  
      // first determine the number of activated vw
      std::set< size_t > activated_visual_words;
      activated_visual_words.clear();
      for( size_t j=0; j<nb_desc_i; ++j )
		activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
      
      // now we assign all descriptors belonging to the visual word to it
      for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
      {
		for( size_t j=0; j<nb_desc_i; ++j )
		{
		  if( *it == descriptor_2_vw_assignments[offset+j] )
		  {
			uint32_t desc_id = uint64_t( descriptors.size() ) / 128;
			for( uint32_t k=uint32_t(j)*128; k < uint32_t(j)*128+128; ++k )
			  descriptors.push_back(feature_infos[i].descriptors[k]);
			vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );
		  }
		}
      }
    }
    else if( mode == 6 )
    {
	  // integer mean per visual word
	  
      // compute for each visual word the mean descriptors, round it to the next integer and store it
	  
      // first determine the number of activated vw
      std::set< size_t > activated_visual_words;
      activated_visual_words.clear();
      for( size_t j=0; j<nb_desc_i; ++j )
		activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
      
      // now we compute the mean descriptors belonging to the visual words
      for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
      {
		std::vector< unsigned char > visual_word_descriptors;
		visual_word_descriptors.clear();
		for( size_t j=0; j<nb_desc_i; ++j )
		{
		  if( *it == descriptor_2_vw_assignments[offset+j] )
		  {
			for( uint32_t k=uint32_t(j)*128; k < uint32_t(j)*128+128; ++k )
			  visual_word_descriptors.push_back(feature_infos[i].descriptors[k]);
		  }
		}
		
		// now compute the mean
		std::vector< float > mean_descriptor;
		
		compute_mean( visual_word_descriptors, mean_descriptor );
		
		// round to the nearest integer values
		std::vector< unsigned char > integer_mean( 128, 0 );
		for( int k=0; k<128; ++k )
		{
		  float bottom =  mean_descriptor[k] - floor( mean_descriptor[k] );
		  float top =  ceil( mean_descriptor[k] ) - mean_descriptor[k];
		  if( bottom < top )
			integer_mean[k] = (unsigned char) floor( mean_descriptor[k] );
		  else
			integer_mean[k] = (unsigned char) ceil( mean_descriptor[k] );
		}
		
		uint32_t desc_id = uint64_t( descriptors.size() ) / 128;
		
		// store the descriptor
		for( uint32_t k=0; k<128; ++k )
		  descriptors.push_back(integer_mean[k]);

		vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );
		
		integer_mean.clear();
		mean_descriptor.clear();
      }
    }
    else if( mode == 1 || mode == 2 )
    {
      // compute the mean descriptor
      std::vector< float > mean_descriptor;
      compute_mean( feature_infos[i].descriptors, mean_descriptor );
      
      // store the descriptor
      uint32_t desc_id = uint64_t( descriptors_float.size() ) / 128;
	
      for( uint32_t k=0; k<128; ++k )
		descriptors_float.push_back(mean_descriptor[k]);
      
      if( mode == 1 )
      {
		std::vector< uint32_t > assignment(1,0);
		// compute the visual word of the mean descriptor and assign it to the word
		vw_handler.set_nb_paths( 10 );
		vw_handler.assign_visual_words_float( mean_descriptor, 1, assignment );

		vw_point_descriptor_idx[ assignment[0] ].push_back(std::make_pair( i, desc_id ) );
      }
      else if( mode == 2 )
      {
		// assign the mean descriptor to all the visual words that the descriptors of the point belong to
		std::set< size_t > activated_visual_words;
		activated_visual_words.clear();
		for( size_t j=0; j<nb_desc_i; ++j )
		  activated_visual_words.insert( descriptor_2_vw_assignments[offset+j] );
		
		// store the reference to the mean descriptor in all activated visual words
		for( std::set< size_t >::iterator it = activated_visual_words.begin(); it != activated_visual_words.end(); ++it )
		  vw_point_descriptor_idx[ *it ].push_back(std::make_pair( i, desc_id ) );
      }
    }
    
    offset += nb_desc_i;
    
  }
  
  std::cout << " done computing the assignments" << std::endl;

  clock_t end3 = clock();

  
  ////
  // output some statistics about the number of activated (= non-empty) visual words
  
  uint32_t nb_non_empty_vw = 0;
  {                                            
    double points_per_vw = 0;
    uint32_t nb_vis_polys = 0;
    
    uint32_t max_polys = 0;
    for( uint32_t i=0; i<nb_cluster; ++i )
    {
      
      if( vw_point_descriptor_idx[i].size() > 0 )
      {
		++nb_non_empty_vw;
		points_per_vw += (double) vw_point_descriptor_idx[i].size();
		max_polys = std::max( max_polys, (uint32_t) vw_point_descriptor_idx[i].size() );
      }
      nb_vis_polys += vw_point_descriptor_idx[i].size();
    }
    
    
    offset = 0;    
    
    std::cout << std::endl << "################ statistics #################" << std::endl;
    std::cout << " #activated vws: " << nb_non_empty_vw << " ( " << double(nb_non_empty_vw)/double(nb_cluster) * 100.0 << " % ) with " << points_per_vw / double(nb_non_empty_vw) << " 3D points on average (for activated polys), max: " << max_polys << std::endl;
    std::cout << " # 3D points : " << nb_points << std::endl;
    std::cout << " # computed medoid descriptors: " << descriptors.size() / 128 << std::endl;
    std::cout << " # computed mean descriptors: " << descriptors_float.size() / 128 << std::endl;
    std::cout << "################ statistics #################" << std::endl << std::endl;
  }
  
  
  clock_t start4 = clock();
  ////
  // write to output
  
  // count the number of assignments
  uint32_t nb_assignments = 0;
  std::cout << "-> saving the descriptor-to-visual word assignments to " << desc_output << std::endl;
  {
	std::ofstream ofs( desc_output.c_str(), std::ios::out | std::ios::binary );
	
	if( !ofs.is_open() )
	{
	  std::cerr << " Could not write the descriptors to file " << desc_output << std::endl;
	  return -1;
	}
	
	uint32_t nb_descriptors = 0;
	if( mode == 0 || mode == 3 || mode == 5 || mode == 6 )
	  nb_descriptors = uint32_t( descriptors.size()/128 );
	else if( mode == 1 || mode == 2 || mode == 4 )
	  nb_descriptors = uint32_t( descriptors_float.size()/128 );
	
	// first write the number of remaining 3D points and the number of vw and the number of non-empty visual words and the number of descriptors
	ofs.write( (char*) &nb_points, sizeof( uint32_t ) );
	ofs.write( (char*) &nb_cluster, sizeof( uint32_t ) );
	ofs.write( (char*) &nb_non_empty_vw, sizeof( uint32_t ) );
	ofs.write( (char*) &nb_descriptors, sizeof( uint32_t ) );
	
	// then the 3D points together
	for( uint32_t i=0; i<nb_points; ++i )
	{
	  float x = feature_infos[i].point.x;
	  float y = feature_infos[i].point.y;
	  float z = feature_infos[i].point.z;
	  ofs.write( (char*) &x, sizeof( float ) );
	  ofs.write( (char*) &y, sizeof( float ) );
	  ofs.write( (char*) &z, sizeof( float ) );
	}
	
	// now the descriptors
	if( mode == 0 || mode == 3 || mode == 5 || mode == 6 )
	{
	  for( uint32_t i=0; i<uint32_t( descriptors.size() ); ++i )
	  {
		unsigned char entry = descriptors[i];
		ofs.write( (char*) &entry, sizeof( unsigned char ) );
	  }
		}
		else if( mode == 1 || mode == 2 || mode == 4 )
		{
	  for( uint32_t i=0; i<uint32_t( descriptors_float.size() ); ++i )
	  {
		float entry = descriptors_float[i];
		ofs.write( (char*) &entry, sizeof( float ) );
	  }
	}
	
	// write out the assignments vw -> ( 3D point id, descriptor id )
	// format: cluster_id nb_assignments assignments (as pairs of uint32_t )
	for( uint32_t i=0; i<nb_cluster; ++i )
	{
	  uint32_t nb_desc_assignments = (uint32_t) vw_point_descriptor_idx[i].size();
	  ofs.write( (char*) &i, sizeof( uint32_t ) );
	  ofs.write( (char*) &nb_desc_assignments, sizeof( uint32_t ) );
	  for( std::vector< std::pair< uint32_t, uint32_t > >::const_iterator it = vw_point_descriptor_idx[i].begin(); it != vw_point_descriptor_idx[i].end(); ++it )
	  {
		uint32_t first = it->first;
		uint32_t second = it->second;
		ofs.write( (char*) &first, sizeof( uint32_t ) );
		ofs.write( (char*) &second, sizeof( uint32_t ) );
		++nb_assignments;
	  }
	}
	
	ofs.close();
  }
  std::cout << "--> done " << std::endl;
  clock_t end4 = clock();
  
  ////
  // display statistics about memory consumption:
  std::cout << "***** Memory requirements ******" << std::endl;
  std::cout << " " << nb_points << " points -> " << nb_points * 3 << " floats -> " << nb_points * uint32_t(3) * uint32_t( sizeof( float ) ) / ( uint32_t(1024) * uint32_t(1024) ) << " MB " << std::endl;
  if( mode == 0 || mode == 3 || mode == 5 || mode == 6 )
    std::cout << " " << uint32_t( descriptors.size()/128 ) << " descriptors -> " << uint32_t( descriptors.size() ) << " unsigned chars -> " << uint32_t( descriptors.size() ) / ( uint32_t(1024) * uint32_t(1024) ) * sizeof( unsigned char ) << " MB " << std::endl;
  else if( mode == 1 || mode == 2 || mode == 4  )
    std::cout << " " << uint32_t( descriptors_float.size()/128 ) << " descriptors -> " << uint32_t( descriptors_float.size() ) << " floats -> " << uint32_t( descriptors_float.size() ) / ( uint32_t(1024) * uint32_t(1024) ) * sizeof( float ) << " MB " << std::endl;
  std::cout << " " << nb_assignments << " assignments -> " << nb_assignments * 2 << " uint32_t -> " << nb_assignments * uint32_t(2) * uint32_t( sizeof( uint32_t ) ) / ( uint32_t(1024) * uint32_t(1024) ) << " MB " << std::endl;
  if( mode == 0 || mode == 3 || mode == 5 || mode == 6 )
    std::cout << " in total : " << nb_assignments * uint32_t(2) * uint32_t( sizeof( uint32_t ) ) / ( uint32_t(1024) * uint32_t(1024) ) + nb_points * uint32_t(3) * uint32_t( sizeof( float ) ) / ( uint32_t(1024) * uint32_t(1024) )  + uint32_t( descriptors.size() ) / ( uint32_t(1024) * uint32_t(1024) ) * sizeof( unsigned char ) << " MB " << std::endl;
  else if( mode == 1 || mode == 2 || mode == 4 )
    std::cout << " in total : " << nb_assignments * uint32_t(2) * uint32_t( sizeof( uint32_t ) ) / ( uint32_t(1024) * uint32_t(1024) ) + nb_points * uint32_t(3) * uint32_t( sizeof( float ) ) / ( uint32_t(1024) * uint32_t(1024) )  + uint32_t( descriptors_float.size() ) / ( uint32_t(1024) * uint32_t(1024) ) * sizeof( float ) << " MB " << std::endl;
  

    printf("[COMPUTE_DESC_ASSI] Reading file took %0.3fs\n", 
            (end1 - start1) / ((double) CLOCKS_PER_SEC));
    printf("[COMPUTE_DESC_ASSI] Assigning VW took %0.3fs\n", 
            (end2 - start2) / ((double) CLOCKS_PER_SEC));
    printf("[COMPUTE_DESC_ASSI] Computing mean etc.  took %0.3fs\n", 
            (end3 - start3) / ((double) CLOCKS_PER_SEC));
    printf("[COMPUTE_DESC_ASSI] Writing file took %0.3fs\n", 
            (end4 - start4) / ((double) CLOCKS_PER_SEC));
    printf("[COMPUTE_DESC_ASSI_OMP] Assigning VW took %0.3fs per point\n", 
            (end2 - start2) / ((double) CLOCKS_PER_SEC * (double)nb_points));
    printf("[COMPUTE_DESC_ASSI_OMP] Computing mean etc.  took %0.3fs per point\n", 
            (end3 - start3) / ((double) CLOCKS_PER_SEC * (double)nb_points));
    printf("[COMPUTE_DESC_ASSI] Total took %0.3fs\n", 
            (end4 - start1) / ((double) CLOCKS_PER_SEC));


    printf("\nDone computing descriptor assignments");
  return 0;
}

