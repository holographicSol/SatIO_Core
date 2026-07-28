/*
    Filter Library. Written by Benjamin Jack Cullen.

    Reusable signal filters. Each filter is a small, caller-owned state
    struct plus init/update functions, so multiple independent instances
    can run side by side (e.g. one per gyro axis).

    Currently includes:
      - KalmanFilter1D : scalar (single-value) Kalman filter.
*/

#ifndef UNIDENTIFIEDSTUDIOS_FILTER_H
#define UNIDENTIFIEDSTUDIOS_FILTER_H

/**
 * @struct KalmanFilter1D
 * @brief State for a scalar (single-value) Kalman filter.
 */
struct KalmanFilter1D {
  double x; // current state estimate
  double p; // current estimate uncertainty (error covariance)
  double q; // process noise variance (expected drift of the true value between updates)
  double r; // measurement noise variance (sensor noise)
  bool initialized; // false until kalmanFilter1DUpdate() has seeded x with a first measurement
};

/**
 * @brief Initializes a scalar Kalman filter's tuning. x is left unseeded
 *        (initialized = false); the first call to kalmanFilter1DUpdate()
 *        seeds x directly from its measurement rather than filtering
 *        against an arbitrary starting value.
 * @param kf Filter to initialize.
 * @param initial_uncertainty Starting estimate uncertainty, used from the
 *        second update onward. Larger values let early measurements
 *        correct the estimate faster.
 * @param process_noise Process noise variance. Larger values let the
 *        filter track a changing signal faster, at the cost of more noise
 *        in the output.
 * @param measurement_noise Measurement noise variance. Larger values trust
 *        new measurements less.
 * @return None.
 */
void kalmanFilter1DInit(struct KalmanFilter1D *kf,
                         double initial_uncertainty,
                         double process_noise,
                         double measurement_noise);

/**
 * @brief Feeds one measurement through the filter and returns the new
 *        estimate. The first call after kalmanFilter1DInit() seeds x with
 *        measurement directly; every call after that predicts (grows p by
 *        q) then updates (blends measurement into x, weighted by relative
 *        uncertainty).
 * @param kf Filter to update.
 * @param measurement Latest raw measurement.
 * @return Filtered state estimate (also stored in kf->x).
 */
double kalmanFilter1DUpdate(struct KalmanFilter1D *kf, double measurement);

#endif /* UNIDENTIFIEDSTUDIOS_FILTER_H */
