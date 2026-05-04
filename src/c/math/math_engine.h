#ifndef MATH_ENGINE_H
#define MATH_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

// Architectural Constants
#define WINDOW_W 60           // 60-second rolling window for live telemetry
#define RESOLUTION_FLOOR 0.01 // Hardware counter variance resolution minimum

// Dynamic Macro-State Modes
typedef enum
{
 MODE_CALIBRATING,
 MODE_ARMED
} SensorMode;

// Expose the global state for the telemetry logger (Action 3)
extern SensorMode current_mode;
extern int macro_state_id;
extern double threat_score;
extern double active_ceiling;
extern double current_mad_live;
extern double current_confidence_c;

// Core Engine Prototypes
void initialize_new_macro_state(void);
void process_telemetry(double current_r1_cpu, double current_r2_io);

// Math Helper Prototypes (Exposed for unit testing)
double calculate_median(const double *array, int n);
double calculate_mad(const double *array, int n, double median);
double calculate_max_burst_integral(const double *array, int n, double median);

#endif // MATH_ENGINE_H