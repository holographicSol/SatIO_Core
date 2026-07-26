/*
    Quaternion - Written By Benjamin Jack Cullen.
*/
#include <cmath>
#include <cstdint>
#include <cstring>
#include <math.h>

#ifndef UNIDENTIFIEDSTUDIOS_QUATERNION_H
#define UNIDENTIFIEDSTUDIOS_QUATERNION_H

/**
 * @brief Quaternion (w,x,y,z), Hamilton convention.
 */
struct Quaternion {
    double w;
    double x;
    double y;
    double z;
};

/**
 * @brief Builds a quaternion from roll/pitch/yaw (radians), using the
 * aerospace ZYX intrinsic (yaw-pitch-roll) convention: R = Rz(yaw)*Ry(pitch)*Rx(roll).
 */
static Quaternion quaternionFromEuler(double roll_rad, double pitch_rad, double yaw_rad);

/**
 * @brief Rotates vector (vx,vy,vz) by quaternion q: v' = q * v * conjugate(q).
 */
static void quaternionRotateVector(const Quaternion &q, double vx, double vy, double vz,
                                    double *out_x, double *out_y, double *out_z);


#endif // UNIDENTIFIEDSTUDIOS_QUATERNION_H