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

/* Camera.h */
/* Camera classes */

#ifndef __camera_h__
#define __camera_h__

#include <stdio.h>

class CameraInfo {
public:
    CameraInfo() { 
        m_adjusted = false; 

        m_constrained[0] = m_constrained[1] = m_constrained[2] = false; 
        m_constrained[3] = m_constrained[4] = m_constrained[5] = false; 
        m_constrained[6] = false;

        m_constraints[0] = m_constraints[1] = m_constraints[2] = 0.0;
        m_constraints[3] = m_constraints[4] = m_constraints[5] = 0.0;
        m_constraints[6] = 0.0;

        m_constraint_weights[0] = 
            m_constraint_weights[1] = 
            m_constraint_weights[2] = 0.0;
        m_constraint_weights[3] = 
            m_constraint_weights[4] = 
            m_constraint_weights[5] = 0.0;
        m_constraint_weights[6] = 0.0;      

        m_k[0] = m_k[1] = 0.0;
        
    }

    /* Finalize the camera */
    void Finalize();
    /* Return the position of the camera */
    
    /* Set the position of the camera */
    void SetPosition(const double *pos);
    /* Return the 3x3 intrinsic matrix */
    void GetIntrinsics(double *K) const;
    /* Project the point into the camera */
    bool Project(const double *p, double *proj) const;
    /* Compute the essential matrix between this camera and another one */

    /* Write params to file*/
    void WriteFile(FILE *f);

    bool m_adjusted;        /* Has this camera been adjusted? */
    double m_focal;         /* Focal length */
    double m_k[2];          /* Distortion parameters */
    double m_R[9], m_t[3];  /* Extrinsics */
    double m_Pmatrix[12];
    int m_width, m_height;  /* Image dimensions */

    /* Horizon line */
    double m_horizon[3];

    double m_RGB_transform[12];   /* Local affine transform for RGB
                                   * space */

    double m_color[3];

    /* Constraints on camera center location */
    bool m_constrained[7];
    double m_constraints[7];
    double m_constraint_weights[7];

};

#endif /* __camera_h__ */
