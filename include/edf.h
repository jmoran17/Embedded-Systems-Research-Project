#ifndef EDF_H
#define EDF_H

// Run EDF for N jobs per task.
// EDF is implemented as a single-threaded superloop (non-preemptive).
int run_edf(int jobs_per_task);

#endif
