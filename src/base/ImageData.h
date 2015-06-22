#ifndef __IMAGEDATACOMPACT_H
#define __IMAGEDATACOMPACT_H 

#include "Camera.h"
#include <vector>
#include <string>

namespace localize {


class Keypoint {
public:    
    Keypoint()  
    { m_x = 0.0; m_y = 0.0; m_extra = -1; m_track = -1; }

    Keypoint(float x, float y) :
	m_x(x), m_y(y)
    { m_r = 0; m_g = 0; m_b = 0; }

    virtual ~Keypoint() {}   
    
    virtual unsigned char *GetDesc() {
        return NULL;
    }

    float m_x, m_y;        /* Subpixel location of keypoint. */
    // float m_scale, m_ori;  /* Scale and orientation (range [-PI,PI]) */
    unsigned char m_r, m_g, m_b;          /* Color of this key */

    int m_extra;  /* 4 bytes of extra storage */
    int m_track;  /* Track index this point corresponds to */
};

class KeypointWithDesc : public Keypoint
{
public:
    virtual ~KeypointWithDesc() {}
    
    virtual unsigned char *GetDesc() {
        return m_d;
    }

    KeypointWithDesc()  
    { m_x = 0.0; m_y = 0.0; m_d = NULL; 
        m_extra = -1; m_track = -1; }

    KeypointWithDesc(float x, float y, unsigned char *d) :
	Keypoint(x, y), m_d(d)
    { }

    unsigned char *m_d;    /* Vector of descriptor values */
};


class ImageData {
public:

    char m_user_name[256];    /* User name */
    char m_flickr_index[256]; /* Flickr index */
    
    char *m_name;             /* Filename */
    char *m_key_name;         /* Key filename */
    
    bool m_image_loaded, m_keys_loaded, m_keys_desc_loaded, 
        m_keys_scale_rot_loaded;
    
    bool m_licensed;         /* Can we actually use this image? */
    
    /* Radial distortion parameters */
    bool m_known_intrinsics;
    double m_K[9];
    //double m_k0, m_k1, m_k2, m_k3, m_k4, m_rd_focal;
    double m_k[5];	

    bool m_fisheye;            /* Is this a fisheye image? */
    double m_fCx, m_fCy;       /* Fisheye center */
    double m_fRad, m_fAngle;   /* Other fisheye parameters */
    double m_fFocal;           /* Desired focal length */

    int m_width, m_height;

    /* Focal length parameters */
    bool m_has_init_focal;   /* Do we have an initial focal length? */
    double m_init_focal;     /* Initial focal length */

    CameraInfo m_camera;     /* Information on the camera used to 
                             * capture this image */
    double m_drop_pt[3];                /* World coords of drop
                                         * position */
    std::vector<KeypointWithDesc> m_keys_desc; /* Keypoints with descriptors */

    std::vector<int> m_visible_points;  /* Indices of points visible
                                         * in this image */
    std::vector<int> m_visible_keys;

    void InitFromString(char *buf, char *path, bool fisheye_by_default);
    void GetBaseName(char *buf);    
    void UndistortKeys();
    void UndistortPoint(double x, double y, 
        double &x_out, double &y_out) const;
    void Tokenize(const std::string &str,
        std::vector<std::string> &tokens, 
        const std::string &delimiters);
    void WriteCamera();
    void WriteTracks();

    int GetWidth();
    int GetHeight();
    void CacheDimensions();


};

}
#endif //__IMAGEDATACOMPACT_H 
