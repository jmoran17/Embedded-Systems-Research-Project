# Real-Time Scheduling Demonstration on Raspberry Pi 5

CFS vs RMS vs EDF — Embedded Systems Scheduling Performance

This project demonstrates and compares three scheduling approaches on a Raspberry Pi 5:

DEFAULT (Linux CFS) — Standard Linux scheduler

RMS (Rate Monotonic Scheduling) — Preemptive SCHED_FIFO real-time threads

EDF (Earliest Deadline First) — Superloop, non-preemptive implementation

The system uses GPIO-driven LEDs to visualize task execution and includes detailed jitter and deadline statistics to illustrate how each scheduler behaves under varying system load.

## Project Overview

Embedded and real-time systems require predictable timing, low jitter, and strict deadline guarantees.
General-purpose operating systems like Linux use CFS (Completely Fair Scheduler), which optimizes fairness—not real-time determinism.

This project provides a side-by-side comparison of:

Real-time vs non-real-time scheduling

Preemptive vs non-preemptive execution models

Behavior under stressed vs unstressed CPU conditions

Jitter and deadline performance for periodic tasks

All three schedulers run the same set of tasks with identical workloads to ensure fair comparison.

## Task Model

Each scheduler runs three periodic tasks, each mapped to a dedicated GPIO pin:

Task	Period	WCET	LED Pin
T1	10 ms	1 ms	GPIO 17
T2	50 ms	2 ms	GPIO 27
T3	100 ms	3 ms	GPIO 22

Each task logs:

Jitter (start time deviation from ideal release time)

Worst-case jitter

Average jitter

Deadline misses

Job count

All deadline misses latch a dedicated alarm LED on GPIO 5.

## Schedulers Implemented
### DEFAULT (Linux CFS)

Each task runs in its own pthread.

Scheduled using the normal Linux time-sharing scheduler.

Not deterministic and heavily affected by system load.

Used to illustrate why CFS cannot guarantee periodic deadlines.

### RMS (Rate Monotonic Scheduling) — Preemptive

Implemented using SCHED_FIFO with fixed static priorities:

Shorter period ⇒ higher priority

Provides deterministic periodic activation

Robust under CPU stress

Demonstrates classical theory: RMS guarantees deadlines when CPU utilization conditions are met

### EDF (Earliest Deadline First) — Non-preemptive Superloop

Selects the ready task with the earliest absolute deadline

Runs tasks in a main loop with absolute wake-up times

Deadline-correct but exhibits higher jitter

Good demonstration of EDF vs RMS behavior under real hardware and OS noise

## Key Results
### Stressed System (heavy load)

CFS: High jitter (up to ~6 ms) and multiple deadline misses

RMS: Extremely low jitter (<0.1 ms), zero misses

EDF: Medium jitter (4–9 ms), no misses

### Unstressed System

CFS: Significantly improved; no misses

RMS: Still excellent and nearly unchanged

EDF: Still higher jitter than RMS, but no misses

### Overall Takeaway:

RMS provides the strongest real-time performance and is barely affected by load

EDF remains correct but shows higher jitter due to its non-preemptive nature

CFS is unreliable for real-time use and fails under stress

## Hardware Requirements

Raspberry Pi 5

LED x4

Task LEDs on GPIO 17, 27, 22

Alarm LED on GPIO 5

Breadboard, resistors, jumper wires

Pinctrl (default on Pi 5) for GPIO control

Optional: Pi-active cooler (highly recommended during RMS / SCHED_FIFO demos)

## Software Requirements

Raspberry Pi OS (64-bit recommended)

GCC / G++

pthreads

Real-time runtime privileges (sudo or adjusted cgroups)

pinctrl command for GPIO

Optional but recommended for best RT performance:

sudo sh -c "echo -1 > /proc/sys/kernel/sched_rt_runtime_us"

## Running the Demo
### DEFAULT Scheduler (CFS)  
sudo ./scheduler_demo --mode default --periods 200

### RMS Scheduler (SCHED_FIFO)  
sudo chrt -f 90 ./scheduler_demo --mode rms --jobs 200

### EDF Scheduler (superloop)  
sudo chrt -f 90 ./scheduler_demo --mode edf --jobs 200


### You can also chain all three:
```
sudo ./scheduler_demo --mode default --periods 200 &&
sudo chrt -f 90 ./scheduler_demo --mode rms --jobs 200 &&
sudo chrt -f 90 ./scheduler_demo --mode edf --jobs 200
```
## Jitter & Deadline Reporting

At the end of each run, the program prints:

Worst-case jitter (ms)

Average jitter (ms)

Deadline misses

Total runtime

All three schedulers output identical statistic formats for easier comparison.

### Example:
```
=== RMS (preemptive SCHED_FIFO) ===

T1_10ms:
  Worst jitter: 0.070 ms
  Avg jitter:   0.004 ms
  Deadline misses: 0
```
## Visualization

To help interpret results, you can plot:

Worst-case jitter (logarithmic bar chart)

Deadline misses (simple bar chart)

Jitter comparisons (line chart per scheduler)

Stressed vs unstressed performance tables

These are ideal for slides or final reports.

## Key Lessons Demonstrated

CFS is not suitable for real-time systems

RMS offers the strongest determinism and stability

EDF is correct but more jitter-sensitive

Preemptive RT scheduling dramatically reduces jitter

Superloop EDF illustrates non-preemptive scheduling trade-offs

System load amplifies the differences between schedulers

GPIO output provides intuitive real-world visualization

## Authors  
Joseph Moran & Nick Cuda
Cal Poly Pomona — Embedded Systems / Real-Time Research Project
2025
