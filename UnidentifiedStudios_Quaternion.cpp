/*
    Quaternion - Written By Benjamin Jack Cullen.
*/
#include <cmath>
#include <cstdint>
#include <cstring>
#include <math.h>
#include "UnidentifiedStudios_Quaternion.h"

// External data - adjust these includes to match your project
extern "C" {
    // Declare your sidereal data structures here or include the proper header
}

double deg2rad(double n) {
  return n * 1.745329252e-2;
}

/**
 * @brief Builds a quaternion from roll/pitch/yaw (radians), using the
 * aerospace ZYX intrinsic (yaw-pitch-roll) convention: R = Rz(yaw)*Ry(pitch)*Rx(roll).
 */
Quaternion quaternionFromEuler(double roll_rad, double pitch_rad, double yaw_rad) {
    double cr = cos(roll_rad * 0.5),  sr = sin(roll_rad * 0.5);
    double cp = cos(pitch_rad * 0.5), sp = sin(pitch_rad * 0.5);
    double cy = cos(yaw_rad * 0.5),   sy = sin(yaw_rad * 0.5);

    Quaternion q;
    q.w = (cy * cp * cr) + (sy * sp * sr);
    q.x = (cy * cp * sr) - (sy * sp * cr);
    q.y = (cy * sp * cr) + (sy * cp * sr);
    q.z = (sy * cp * cr) - (cy * sp * sr);
    return q;
}

/**
 * @brief Rotates vector (vx,vy,vz) by quaternion q: v' = q * v * conjugate(q).
 */
void quaternionRotateVector(const Quaternion &q, double vx, double vy, double vz,
                                    double *out_x, double *out_y, double *out_z) {
    double tx = 2.0 * ((q.y * vz) - (q.z * vy));
    double ty = 2.0 * ((q.z * vx) - (q.x * vz));
    double tz = 2.0 * ((q.x * vy) - (q.y * vx));

    *out_x = vx + (q.w * tx) + ((q.y * tz) - (q.z * ty));
    *out_y = vy + (q.w * ty) + ((q.z * tx) - (q.x * tz));
    *out_z = vz + (q.w * tz) + ((q.x * ty) - (q.y * tx));
}