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

/* Camera.cpp */
/* Camera classes */

#include <string.h>
#include <math.h>

#include "Camera.h"

//#include "defines.h"
#include "matrix.h"
#include "vector.h"



/* Finalize the camera */
void CameraInfo::Finalize() {
    /* Compute projection matrix */
    double K[9];
    double Ptmp[12] = 
	{ m_R[0], m_R[1], m_R[2], m_t[0],
	  m_R[3], m_R[4], m_R[5], m_t[1],
	  m_R[6], m_R[7], m_R[8], m_t[2] };
	    
    GetIntrinsics(K);

    matrix_product(3, 3, 3, 4, K, Ptmp, m_Pmatrix);
}


/* Set the position of the camera */
void CameraInfo::SetPosition(const double *pos) {
    matrix_product(3, 3, 3, 1, m_R, (double *) pos, m_t);
    matrix_scale(3, 1, m_t, -1.0, m_t);
}



/* Return the 3x3 intrinsic matrix */
void CameraInfo::GetIntrinsics(double *K) const {
    K[0] = m_focal; K[1] = 0.0;     K[2] = 0.0;
    K[3] = 0.0;     K[4] = m_focal; K[5] = 0.0;
    K[6] = 0.0;     K[7] = 0.0;     K[8] = 1.0;
}

/* Project the point into the camera */
bool CameraInfo::Project(const double *p, double *proj) const
{
    double p4[4] = { p[0], p[1], p[2], 1.0 };
    double proj3[3];

    matrix_product(3, 4, 4, 1, (double *) m_Pmatrix, p4, proj3);

    if (proj3[2] == 0.0)
        return false;

    proj[0] = proj3[0] / -proj3[2];
    proj[1] = proj3[1] / -proj3[2];

    if (m_k[0] == 0.0 && m_k[1] == 0.0)
        return (proj3[2] < 0.0);

    double rsq = (proj[0] * proj[0] + proj[1] * proj[1]) / (m_focal * m_focal);
    double factor = 1.0 + m_k[0] * rsq + m_k[1] * rsq * rsq;

    if (rsq > 8.0 || factor < 0.0) // bad extrapolation
        return (proj[2] < 0.0);

    proj[0] *= factor;
    proj[1] *= factor;
    
    return (proj3[2] < 0.0);
}

