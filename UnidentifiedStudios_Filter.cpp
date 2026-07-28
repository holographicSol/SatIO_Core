/*
    Filter Library. Written by Benjamin Jack Cullen.
*/
#include "UnidentifiedStudios_Filter.h"

void kalmanFilter1DInit(struct KalmanFilter1D *kf,
                         double initial_uncertainty,
                         double process_noise,
                         double measurement_noise) {
  kf->x = 0.0;
  kf->p = initial_uncertainty;
  kf->q = process_noise;
  kf->r = measurement_noise;
  kf->initialized = false;
}

double kalmanFilter1DUpdate(struct KalmanFilter1D *kf, double measurement) {
  if (!kf->initialized) {
    kf->x = measurement;
    kf->initialized = true;
    return kf->x;
  }

  // Predict: constant model, so the estimate itself doesn't move; only its
  // uncertainty grows by the process noise.
  kf->p += kf->q;

  // Update: blend the prediction with the new measurement, weighted by
  // their relative uncertainty.
  double k = kf->p / (kf->p + kf->r); // Kalman gain
  kf->x += k * (measurement - kf->x);
  kf->p *= (1.0 - k);

  return kf->x;
}
