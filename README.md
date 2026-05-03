# Technical Overview & Plan
### **Project Overview**

**Title:** Covert side-channel attack detection protocol in embedded firmware update process. 
**Objective:** To develop a hybrid C/Bash detection engine that monitors hardware-level performance telemetry to identify and halt TOCTOU firmware manipulation attacks in real-time during an embedded device's over-the-air update process.

---

### **The Strict Technology Stack**

We will operate exclusively within these defined boundaries:

- **Physical Hardware (Prod):** FriendlyElec NanoPi R3S (Hardware Rev 1.0, ARM Cortex-A55). Chosen for its authentic flash memory I/O and deep silicon-level PMUv3 performance counters.
    
- **Sandbox Hardware (Dev):** Linux VM / AMD Thin Client for x86 local prototyping.
    
- **Operating System:** OpenWrt 23.05.x (Kernel 5.15+). Provides the network stack and the volatile `tmpfs` staging environment for the attack vector.
    
- **Firmware Delivery Agent:** OpenWrt `sysupgrade`. The baseline update framework and the primary "victim" process executing the volatile-to-non-volatile memory transfer.
    
- **Target Monitoring System:** Linux `perf` subsystem (`perf_event_open` API). Acts as the telemetry extraction engine, reading high-resolution micro-architectural events (e.g., context switches, cache misses) without disrupting execution timing.
    
- **Target Detection Engine:** Hybrid C/C++ (compiled via GCC 11.x/12.x) and GNU Bash 5.1+. C handles high-speed telemetry and threshold mathematics; Bash acts as the native system orchestrator and executes the halting logic.
    
- **Attack Simulation Tool:** `stress-ng` (v0.15.x). Generates the deterministic micro-architectural footprint required to scientifically calibrate the detection thresholds.
    

---

### **Actionable Execution Plan**

#### **Phase 1: Local Prototyping (The x86 Sandbox)**

_Action: Prove the core mechanics—reading silicon and killing a process._

1. **Environment Configuration:** Spin up the x86 Linux VM, install kernel headers (`linux-tools-common`, `build-essential`), and mount a `tmpfs` directory to mimic OpenWrt's `/tmp`.
    
2. **Mock Upgrade Script:** Draft `mock_upgrade.sh` to stage a dummy payload in `tmpfs`, introduce a 5-second `sleep` (simulating the vulnerable TOCTOU window), and attempt a simulated flash write.
    
3. **Barebones Telemetry:** Write `sensor.c` using `<linux/perf_event.h>` to read standard CPU cycles, printing raw counter values every 100ms.
    
4. **IPC Kill Switch:** Update `sensor.c` to artificially exit after a set number of readings. Update `mock_upgrade.sh` to capture the sensor's background PID and issue a `SIGKILL` if the sensor triggers, outputting "ATTACK HALTED!".
    

#### **Phase 2: Mathematical Calibration & Simulation (x86)**

_Action: Synthesize the anomaly and implement the detection math._

1. **Controlled Attack Synthesis:** Write `attack.sh` utilizing `stress-ng --switch 1000 --vm 2` to run for exactly 3 seconds against the mock upgrade process.
    
2. **Rolling Math:** Upgrade `sensor.c` to maintain an array of the last 50 telemetry readings, calculating the rolling mean and standard deviation in real-time.
    
3. **Threshold Lock-in:** Implement the trigger logic: `If current reading > Mean + (3 * Standard Deviation)`, fire the exit code. We will iteratively tune this multiplier to eliminate false positives.
    

#### **Phase 3: The ARM Migration (NanoPi R3S)**

_Action: Cross-compile and port the engine to the physical router._

1. **Hardware Bring-up:** Flash FriendlyWrt/OpenWrt 23.05 to the NanoPi via MicroSD. Verify ARM PMUv3 visibility via `dmesg` and install `stress-ng` via `opkg`.
    
2. **Cross-Compilation:** Configure the OpenWrt ARMv8 GCC toolchain locally and compile `sensor.c` into a native ARM binary (`sensor_arm`).
    
3. **Deployment & Dry Run:** Secure copy (`scp`) the binary and scripts to the NanoPi and execute a dry run to validate ARM counter extraction and Bash IPC.
    

#### **Phase 4: On-Target Validation & Data Harvesting**

_Action: Final calibration and harvesting telemetry for your final report (Problem Description, Data, Solution, Model Evaluation, and Summary)._

1. **ARM Baseline Recalibration:** Run clean upgrades to establish the Cortex-A55 baseline. Adjust the standard deviation multiplier in C to account for the new hardware profile.
    
2. **Live Fire & Recording:** Execute the finalized defense engine against a live `stress-ng` attack. Simultaneously record a flawless video of the dual-SSH terminals (update vs. attack) as your presentation safety net.
    
3. **Telemetry Harvesting:** Ensure the system outputs pristine, timestamped CSV files (`clean_baseline.csv` and `attack_detected.csv`). This data will directly populate the Evaluation and Data chapters of your diploma thesis.
    

#### **Phase 5: Defense Day Preparation**

_Action: Secure the presentation environment._

1. **The Physical Rig:** Hardwire the presentation laptop to the NanoPi via Ethernet using static IPs for an unbreakable SSH connection.
    
2. **Live Demo Choreography:** Set up dual-pane terminal windows to visually contrast the `sysupgrade` execution with the injected `stress-ng` attack, demonstrating the real-time halt.
    
3. **Safety Net Protocol:** Have the Phase 4 video queued and ready to play seamlessly in the event of any hardware or network failures during the defense.
