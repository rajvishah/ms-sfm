/* 
 *  Copyright (c) 2008-2010  Noah Snavely (snavely (at) cs.cornell.edu)
 *    and the University of Washington
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

/* ImageData.cpp */
/* Simple image storage and operations */

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ImageSize.h"
#include "ImageData.h"
//#include "Register.h"


#include "matrix.h"
//#include "pgm.h"
//#include "resample.h"
//#include "transform.h"
#include "util.h"
#define M_PI 3.14159265358979323846f
#define DEG2RAD(d) ((d) * (M_PI / 180.0))


#define SUBSAMPLE_LEVEL 1

#if 1
#define FOCAL_LENGTH 490
#define NUM_ROTATIONS 1
#define DEFAULT_WIDTH 1536
#define DEFAULT_HEIGHT 1024
#else
#define FOCAL_LENGTH 700 // 661.62760416666674
#define NUM_ROTATIONS 0
#define DEFAULT_WIDTH 2074
#define DEFAULT_HEIGHT 1382
#endif

extern double global_scale;

using namespace localize;

/* Initialize the image data given a string description */
void ImageData::InitFromString(char *buf, char *path, bool fisheye_by_default) 
{
    /* Eat the newline */
    if (buf[strlen(buf)-1] == '\n')
        buf[strlen(buf)-1] = 0;

    if (buf[strlen(buf)-1] == '\r')
        buf[strlen(buf)-1] = 0;

    /* Split the buffer into tokens */
    std::string str(buf);
    std::vector<std::string> toks;
    Tokenize(str, toks, " ");

#if 0
    while (t.HasMoreTokens()) {
        tok = t.GetNextToken();
        toks.push_back(tok);
    }
#endif

    int num_toks = (int) toks.size();

    bool fisheye = fisheye_by_default;
    if (num_toks >= 2) {
        fisheye = (atoi(toks[1].c_str()) == 1);
    }

    bool has_init_focal = false;
    double init_focal = 0.0;
    if (num_toks >= 3) {
        has_init_focal = true;
        init_focal = atof(toks[2].c_str());
    }

    ImageData data;

    // printf("[ImageData::InitFromFile] Adding image %s\n", toks[0].c_str());

    if (path == NULL || strcmp(path, ".") == 0 || toks[0].c_str()[0] == '/') {
        m_name = strdup(toks[0].c_str());
    } else {
        char tmp_name[512];
        sprintf(tmp_name, "%s/%s", path, toks[0].c_str());
        m_name = strdup(tmp_name);
    }

    // m_wximage = NULL;
    m_image_loaded = false;
    m_keys_loaded = false;
    m_keys_scale_rot_loaded = false;

    m_fisheye = fisheye;
    m_has_init_focal = has_init_focal;
    m_init_focal = init_focal;
    m_camera.m_adjusted = false;

    m_known_intrinsics = false;
    /* Extract out the user name */
    char base_name[256];
    GetBaseName(base_name);

    char *name_end = base_name;
    // char user[256];

    while (*name_end != '_' && *name_end != 0)
        name_end++;

    if (*name_end == 0) {
        strcpy(m_user_name, "Unknown");
        strcpy(m_flickr_index, "Unknown");
    } else {
        strncpy(m_user_name, base_name, name_end - base_name);
        m_user_name[name_end - base_name] = 0;

        strcpy(m_flickr_index, name_end + 1);

        // for (int i = 0; m_user_name[i] != 0; i++) 
        //    m_user_name[i] = (m_user_name[i] - 'a' + 10) % 26 + 'a';
    }

    /* Try to find a keypoint file */
    char key_buf[256];
    strcpy(key_buf, m_name);
    key_buf[strlen(m_name) - 3] = 'k';
    key_buf[strlen(m_name) - 2] = 'e';
    key_buf[strlen(m_name) - 1] = 'y';

    m_key_name = strdup(key_buf);
    CacheDimensions();
}

int ImageData::GetWidth() { 
	    return m_width;
}

int ImageData::GetHeight() { 
	    return m_height;
}

void ImageData::GetBaseName(char *buf) {
    int len = strlen(m_name);
    
    int i;
    for (i = len - 1; i >= 0 && m_name[i] != '/'; i--) { }

    if (i == -1)
	strcpy(buf, m_name);
    else
	strcpy(buf, m_name + i + 1);
    
    buf[strlen(buf) - 4] = 0;
}

/* Create a pinhole view for the keys */
void ImageData::UndistortKeys() {
    if (!m_fisheye)
	return;

    int num_keys = (int) m_keys_desc.size();
    if (num_keys == 0)
	return;
    
    for (int i = 0; i < num_keys; i++) {
	double x = m_keys_desc[i].m_x;
	double y = m_keys_desc[i].m_y;
	double x_new, y_new;

	UndistortPoint(x, y, x_new, y_new);
	
	m_keys_desc[i].m_x = x_new;
	m_keys_desc[i].m_y = y_new;
    }
}

void ImageData::UndistortPoint(double x, double y, 
			       double &x_out, double &y_out) const
{
    if (!m_fisheye) {
	// UndistortPointRD(x, y, x_out, y_out);

	x_out = x;
	y_out = y;

	return;
    }

    double xn = x - m_fCx;
    double yn = y - m_fCy;
    
    double r = sqrt(xn * xn + yn * yn);
    double angle = 0.5 * m_fAngle * (r / m_fRad);
    double rnew = m_fFocal * tan(DEG2RAD(angle));
    
    x_out = xn * (rnew / r); // + m_fCx;
    y_out = yn * (rnew / r); // + m_fCy;
}

void ImageData::WriteTracks()
{
    char cam_buf[256];
    strcpy(cam_buf, m_name);
    cam_buf[strlen(m_name) - 3] = 't';
    cam_buf[strlen(m_name) - 2] = 'r';
    cam_buf[strlen(m_name) - 1] = 'k';

    FILE *f = fopen(cam_buf, "w");
    
    if (f == NULL) {
	printf("[ImageData::WriteTracks] Error opening file %s for writing\n",
	       cam_buf);
	return;
    }

    /* Count the number of tracks */
    int num_keys = (int) m_keys_desc.size();
    int num_tracks = 0;
    
    for (int i = 0; i < num_keys; i++)
	if (m_keys_desc[i].m_extra != -1)
	    num_tracks++;

    fprintf(f, "%d\n", num_tracks);
    
    /* Write the tracks */
    for (int i = i; i < num_keys; i++)
	if (m_keys_desc[i].m_extra != -1)
	    fprintf(f, "%d %d\n", i, m_keys_desc[i].m_extra);

    fclose(f);
}

/* Write the camera */
void ImageData::WriteCamera()
{
    char cam_buf[256];
    strcpy(cam_buf, m_name);
    cam_buf[strlen(m_name) - 3] = 'c';
    cam_buf[strlen(m_name) - 2] = 'a';
    cam_buf[strlen(m_name) - 1] = 'm';

    FILE *f = fopen(cam_buf, "w");
    
    if (f == NULL) {
	printf("[ImageData::WriteCamera] Error opening file %s for writing\n",
	       cam_buf);
	return;
    }
    
    fprintf(f, "%0.8e %0.8e %0.8e\n", 
            m_camera.m_focal, m_camera.m_k[0], m_camera.m_k[1]);
    fprintf(f, "%0.8e %0.8e %0.8e\n", 
	    m_camera.m_R[0], m_camera.m_R[1], m_camera.m_R[2]);
    fprintf(f, "%0.8e %0.8e %0.8e\n", 
	    m_camera.m_R[3], m_camera.m_R[4], m_camera.m_R[5]);
    fprintf(f, "%0.8e %0.8e %0.8e\n", 
	    m_camera.m_R[6], m_camera.m_R[7], m_camera.m_R[8]);
    fprintf(f, "%0.8e %0.8e %0.8e\n", 
	    m_camera.m_t[0], m_camera.m_t[1], m_camera.m_t[2]);

    fclose(f);
}

void ImageData::Tokenize(const std::string& str,
              std::vector<std::string>& tokens,
              const std::string& delimiters)
{
    /* Skip delimiters at beginning */
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    /* Find first "non-delimiter" */
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        /* Found a token, add it to the vector */
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        /* Skip delimiters.  Note the "not_of" */
        lastPos = str.find_first_not_of(delimiters, pos);
        /* Find next "non-delimiter" */
        pos = str.find_first_of(delimiters, lastPos);
    }
}


void ImageData::CacheDimensions() 
{
    int w = 1504, h = 1000;
	int len = strlen(m_name);
    if (strcmp(m_name + len - 3, "jpg") == 0) {
        string imageName(m_name);
        ImageSize::GetSize(imageName, &w, &h);
    } else {
        printf("\nNonjpeg image found");
        exit(-1);
    }
    m_width = w;
    m_height = h;
}
