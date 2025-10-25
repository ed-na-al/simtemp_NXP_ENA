## **High-Level Test Plan**

This plan validates the main acceptance criteria for the software engineer challenge.

### **T1 — Module Life Cycle (Load/Unload)**

| Objective | Description | Commands | Success Criteria |
| :---- | :---- | :---- | :---- |
| **T1.1** Build | Compile the module and the user application. | ./scripts/build.sh | Successful compilation (nxp\_simtemp.ko generated). |
| **T1.2** Load | Insert the module. | sudo insmod kernel/nxp\_simtemp.ko | No errors in dmesg. The nodes /dev/simtemp0 and /sys/class/simtemp/simtemp0 exist. |
| **T1.3** SysFS Defaults | Verify reading the initial values. | cat /sys/.../sampling\_ms | Values match the defaults (e.g., 1000) or those defined in the DT (e.g., 500). |
| **T1.4** Unload (Clean) | Remove the module. | sudo rmmod nxp\_simtemp | Module removed without "Device is busy" error. No warnings/OOPS in dmesg. The nodes are gone. |

### **T2 — Data Path and Frequency**

| Objective | Description | Commands | Success Criteria |
| :---- | :---- | :---- | :---- |
| **T2.1** Binary Read | Verify that the CLI can read a complete binary record. | ./user/cli/main.py | The CLI correctly decodes 8 \+ 4 \+ 4 \= 16 bytes. The timestamp\_ns and temp\_mC fields are valid. |
| **T2.2** Frequency | Verify that the sampling frequency is respected. | ./user/cli/main.py \--sampling 200 | The output shows approximately 5 readings per second. |
| **T2.3** Blocking | Verify blocking of read() and poll() without O\_NONBLOCK. | dd if=/dev/simtemp0 bs=16 count=1 | dd blocks until a new sample is available. |

### **T3 — Alert Threshold and Events (POLLPRI)**

| Objective | Description | Commands | Success Criteria |
| :---- | :---- | :---- | :---- |
| **T3.1** Threshold Event | Configure a low threshold to force an alert. | echo 30000 \> /sys/.../threshold\_mc | The CLI (using poll()) detects a **POLLPRI** event. The binary record read has the SIMTEMP\_FLAG\_THRESHOLD\_CROSSED (bit 1) set. |
| **T3.2** Test Mode | Execute the automatic CLI test. | ./scripts/run\_demo.sh | The script finishes with a TEST: PASS message and exit code 0. |
| **T3.3** Alert Deactivated | Raise the threshold above the simulated temperature. | echo 60000 \> /sys/.../threshold\_mc | No POLLPRI events are triggered. |

### **T4 — SysFS Configuration**

| Objective | Description | Commands | Success Criteria |
| :---- | :---- | :---- | :---- |
| **T4.1** SysFS Write | Modify sampling\_ms to a new value. | echo 50 \> /sys/.../sampling\_ms | The change is applied and the sampling frequency increases immediately. |
| **T4.2** Input Validation | Attempt to write an invalid value. | echo "abc" \> /sys/.../sampling\_ms | The write fails and returns \-EINVAL to the user (the echo command fails). The previous value is maintained. |
| **T4.3** Stats | Verify the increment of counters. | 1\. cat stats. 2\. Wait 5s. 3\. cat stats. | total\_updates increases by \~5x. total\_alerts increases if the threshold is crossed. |
| **T4.4** Mode | Change the simulation mode. | echo noisy \> /sys/.../mode | The simulated temperature shows more variation (noise) in the CLI readings. |

### **T5 — Robustness (Concurrency & Error Paths)**

| Objective | Description | Commands | Success Criteria |
| :---- | :---- | :---- | :---- |
| **T5.1** KFIFO Overflow | Attempt to saturate the buffer (e.g., sampling\_ms=10). | cat stats | total\_overflows should increment. The system should remain stable. |
| **T5.2** Unload Under Load | Remove the module while the CLI is reading. | sudo rmmod while the CLI is running. | rmmod must succeed. The kernel must not crash/WARN/BUG (no OOPS). |
