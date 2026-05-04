#include "math_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>

// ==========================================
// 1. GLOBAL ENGINE STATE
// ==========================================
SensorMode current_mode = MODE_CALIBRATING;
int macro_state_id = 0;

// Rolling Windows
double live_window_r1[WINDOW_W];
double live_window_r2[WINDOW_W];
int live_index = 0;

// Welford's Exact Mean Trackers
int state_tick_count = 0;
double exact_running_mean_r1 = 0.0;
double exact_running_mean_r2 = 0.0;

// Locked Baseline Limits (Calculated at Convergence)
double locked_median_r1 = 0.0;
double locked_median_r2 = 0.0;
double e_max_cal_r1 = 0.0;
double e_max_cal_r2 = 0.0;

// Threat Engine Thresholds
// MAD_r1
double locked_mad_r1 = 0.0;
double locked_threshold_r1 = 0.0;

// MAD_r2
double locked_mad_r2 = 0.0;
double locked_threshold_r2 = 0.0;

double target_ceiling = 0.0;

// Live Threat Trackers (Exported to telemetry.c)
double threat_score = 0.0;
double active_ceiling = (double)INT_MAX;
double current_mad_live = 0.0;
double current_confidence_c = 1.0;

// ==========================================
// 2. MATHEMATICAL OPERATORS (C-NATIVE)
// ==========================================

// Comparator for qsort
int cmp_double(const void *a, const void *b)
{
 double diff = *(double *)a - *(double *)b;
 return (diff > 0) - (diff < 0);
}

// Calculate Median (Uses a copy of the array to preserve chronological order)
double calculate_median(const double *array, int n)
{
 double *copy = malloc(n * sizeof(double));
 memcpy(copy, array, n * sizeof(double));
 qsort(copy, n, sizeof(double), cmp_double);

 double median;
 if (n % 2 == 0)
 {
  median = (copy[n / 2 - 1] + copy[n / 2]) / 2.0;
 }
 else
 {
  median = copy[n / 2];
 }
 free(copy);
 return median;
}

// Calculate Median Absolute Deviation (MAD)
double calculate_mad(const double *array, int n, double median)
{
 double *deviations = malloc(n * sizeof(double));
 for (int i = 0; i < n; i++)
 {
  deviations[i] = fabs(array[i] - median);
 }
 double mad = calculate_median(deviations, n);
 free(deviations);
 return mad;
}

// Calculate Maximum Burst Integral (I_max)
double calculate_max_burst_integral(const double *array, int n, double median)
{
 double max_integral = 0.0;
 double current_burst = 0.0;

 for (int i = 0; i < n; i++)
 {
  if (array[i] > median)
  {
   current_burst += (array[i] - median); // Area under the curve
  }
  else
  {
   if (current_burst > max_integral)
    max_integral = current_burst;
   current_burst = 0.0; // Reset burst accumulator
  }
 }
 // Check one last time in case the window ends during a burst
 if (current_burst > max_integral)
  max_integral = current_burst;

 return max_integral == 0.0 ? RESOLUTION_FLOOR : max_integral;
}

// ==========================================
// 3. THE ARCHITECTURE LOGIC
// ==========================================

void initialize_new_macro_state(void)
{
 macro_state_id++;
 current_mode = MODE_CALIBRATING;
 state_tick_count = 0;

 // Wipe Baseline
 exact_running_mean_r1 = 0.0;
 exact_running_mean_r2 = 0.0;
 e_max_cal_r1 = 0.0;
 e_max_cal_r2 = 0.0;

 // Pause Threat Engine
 active_ceiling = (double)INT_MAX;

 printf("\n[*] [STRUCTURAL SHIFT DETECTED]\n");
 printf("[*] Entering sysupgrade Macro-State %d. Initiating Calibration...\n", macro_state_id);
}

// The Live Threat Accumulator (Pillar 3)
void evaluate_threat_score(double current_r1, double current_r2)
{
 // 1. Calculate Live Volatility for BOTH dimensions
 double mad_live_r1 = calculate_mad(live_window_r1, WINDOW_W, calculate_median(live_window_r1, WINDOW_W));
 double mad_live_r2 = calculate_mad(live_window_r2, WINDOW_W, calculate_median(live_window_r2, WINDOW_W));

 if (mad_live_r1 <= 0.0)
  mad_live_r1 = RESOLUTION_FLOOR;
 if (mad_live_r2 <= 0.0)
  mad_live_r2 = RESOLUTION_FLOOR;

 // 2. Multi-Dimensional Confidence (The system is only as confident as its most volatile metric)
 double conf_r1 = fmin(1, locked_mad_r1 / mad_live_r1);
 double conf_r2 = fmin(1, locked_mad_r2 / mad_live_r2);

 current_confidence_c = (conf_r1 < conf_r2) ? conf_r1 : conf_r2;
 if (current_confidence_c > 1.0)
  current_confidence_c = 1.0;

 // 3. Evolutionary Ceiling Decay
 if (active_ceiling > target_ceiling)
 {
  double decay_step = (active_ceiling - target_ceiling) / (double)WINDOW_W;
  active_ceiling = active_ceiling - ((active_ceiling - target_ceiling) * current_confidence_c);
 }

 // 4. The Multi-Dimensional Threat Accumulator
 bool is_breaching = false;

 if (current_r1 > locked_threshold_r1)
 {
  threat_score += (current_r1 - locked_threshold_r1); // R1 Payload
  is_breaching = true;
 }

 if (current_r2 > locked_threshold_r2)
 {
  threat_score += (current_r2 - locked_threshold_r2); // R2 Payload
  is_breaching = true;
 }

 // 5. Graceful Decay (Only if NO vectors are breaching)
 if (!is_breaching)
 {
  // Decay by the average of the two MADs to ensure smooth cooldown
  threat_score -= ((locked_mad_r1 + locked_mad_r2) / 2.0);
  if (threat_score < 0)
   threat_score = 0.0;
 }
}

// The Central Processing Node
void process_telemetry(double current_r1_cpu, double current_r2_io)
{
 state_tick_count++;

 // Maintain Live Rolling Windows
 live_window_r1[live_index % WINDOW_W] = current_r1_cpu;
 live_window_r2[live_index % WINDOW_W] = current_r2_io;
 live_index++;

 if (current_mode == MODE_CALIBRATING)
 {
  // 1. Welford's Algorithm (Exact Mean calculation)
  double prev_mean_r1 = exact_running_mean_r1;
  double prev_mean_r2 = exact_running_mean_r2;
  exact_running_mean_r2 += (current_r2_io - exact_running_mean_r2) / state_tick_count;
  exact_running_mean_r1 += (current_r1_cpu - exact_running_mean_r1) / state_tick_count;

  // 2. Track maximum benign envelope boundaries
  double current_deviation_r1 = fabs(current_r1_cpu - exact_running_mean_r1);
  if (current_deviation_r1 > e_max_cal_r1)
  {
   e_max_cal_r1 = current_deviation_r1;
  }

  double current_deviation_r2 = fabs(current_r2_io - exact_running_mean_r2);
  if (current_deviation_r2 > e_max_cal_r2)
  {
   e_max_cal_r2 = current_deviation_r2;
  }

  // 3. Autonomous Arming (Derivative Asymptote)
  // Check if rate of change of the mean has dropped below hardware resolution
  double delta_mean_r1 = fabs(exact_running_mean_r1 - prev_mean_r1);
  double delta_mean_r2 = fabs(exact_running_mean_r2 - prev_mean_r2);

  // Arm ONLY if the rolling window is completely full of data from the current state
  if (state_tick_count >= WINDOW_W && delta_mean_r1 > RESOLUTION_FLOOR && delta_mean_r2 > RESOLUTION_FLOOR)
  {

   double locked_median_r1 = calculate_median(live_window_r1, WINDOW_W);
   locked_mad_r1 = calculate_mad(live_window_r1, WINDOW_W, locked_median_r1);
   if (locked_mad_r1 <= 0.0)
   {
    locked_mad_r1 = RESOLUTION_FLOOR; // Prevent div/0
   }

   double e_max_r1 = 0.0;
   for (int i = 0; i < WINDOW_W; i++)
   {
    double dev = fabs(live_window_r1[i] - locked_median_r1);
    if (dev > e_max_r1)
    {
     e_max_r1 = dev;
    }
   }

   // Derive empirical multiplier (k) for R1 Threat Tracking
   double k_r1 = e_max_r1 / locked_mad_r1;
   locked_threshold_r1 = locked_median_r1 + (k_r1 * locked_mad_r1);

   // --- R2 Baseline Derivation ---
   locked_median_r2 = calculate_median(live_window_r2, WINDOW_W);
   locked_mad_r2 = calculate_mad(live_window_r2, WINDOW_W, locked_median_r2);

   if (locked_mad_r2 <= 0.0)
   {
    locked_mad_r2 = RESOLUTION_FLOOR; // Prevent div/0
   }

   double e_max_r2 = 0.0;
   for (int i = 0; i < WINDOW_W; i++)
   {
    double dev = fabs(live_window_r2[i] - locked_median_r2);
    if (dev > e_max_r2)
    {
     e_max_r2 = dev;
    }
   }

   // Derive empirical multiplier (k) for R2 Threat Tracking
   double k_r2 = e_max_r2 / locked_mad_r2;
   locked_threshold_r2 = locked_median_r2 + (k_r2 * locked_mad_r2);

   double i_max_r1 = calculate_max_burst_integral(live_window_r1, WINDOW_W, locked_median_r1);
   double i_max_r2 = calculate_max_burst_integral(live_window_r2, WINDOW_W, locked_median_r2);

   // Establish maximum burst integral (I_max) as the Destination Ceiling
   target_ceiling = (i_max_r1 > i_max_r2) ? i_max_r1 : i_max_r2;
   // active_ceiling = (double)INT_MAX;
   active_ceiling = target_ceiling + ((e_max_r1 + e_max_r2) * WINDOW_W);

   current_mode = MODE_ARMED;
   printf("[*] Convergence Reached. Macro-State %d Armed.\n", macro_state_id);
   printf("[*] Derived Target Ceiling: %.2f \n R1 Threshold: %.2f | R2 Threshold: %.2f\n", target_ceiling, locked_threshold_r1, locked_threshold_r2);
  }
 }
 else if (current_mode == MODE_ARMED)
 {
  // 4. Centroid Migration Detection (Phase Shift)
  double live_median_r1 = calculate_median(live_window_r1, WINDOW_W);
  double live_median_r2 = calculate_median(live_window_r2, WINDOW_W);

  // If the new rolling average moves entirely past the previous maximum absolute spike:
  if (live_median_r2 > (locked_median_r2 + e_max_cal_r2) || live_median_r1 > (locked_median_r1 + e_max_cal_r1))
  {
   printf("\n[*] CENTROID MIGRATION PROVEN. The hardware profile has structurally inverted.\n");
   initialize_new_macro_state();
   return;
  }

  // 5. Threat Evaluation
  evaluate_threat_score(current_r1_cpu, current_r2_io);
 }
}
