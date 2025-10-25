# **Documentation: Command Line Interface (CLI) Application**

The main.py script is the Python user interface designed to interact with the nxp\_simtemp kernel driver in three main ways:

1. **Data Reading:** Reads and decodes the binary stream from /dev/simtemp0.
2. **Alert Monitoring:** Uses poll() to wait for data events (POLLIN) and alert events (POLLPRI).
3. **Configuration:** Allows setting the threshold and sampling period through the SysFS files.

## **1\. Requirements**

* Python 3.x
* Permissions to read/write SysFS (requires sudo for writing) and read the device node (/dev/simtemp0).

## **2\. General Usage**

The script supports several arguments to control its behavior.

python3 ./user/cli/main.py \--help

### **Arguments:**

| Argument | Description | Type | Default |
| :---- | :---- | :---- | :---- |
| \-h, \--help | Shows the help message. | N/A | N/A |
| \-t, \--timeout | Wait time in seconds for poll() before reporting a *timeout*. | float | 10.0 |
| \-s, \--sampling | **OPTIONAL**. Configures the sampling\_ms value in SysFS before starting to read. Requires root permissions. | int (ms) | Not configured |
| \-d, \--threshold | **OPTIONAL**. Configures the threshold\_mc value in SysFS before starting to read. Requires root permissions. | int (mC) | Not configured |
| \--test | **Test Mode**. Configures the threshold to force an alert, waits a maximum of two sampling periods, and returns 0 if the alert (POLLPRI) was detected. | flag | Disabled |

## **3\. Operating Modes**

### **3.1 Continuous Reading**

The default mode simply waits for data and prints it, showing both the kernel *timestamp* and the decoded temperature.

\# Read every 5 seconds (assuming sampling\_ms=5000 in the driver)
python3 ./user/cli/main.py
\# Read every 500ms and change the threshold to 40°C (40000 mC)
sudo python3 ./user/cli/main.py \--sampling 500 \--threshold 40000

**Output Format:**

timestamp \= 2025-10-22T00:41:06.661Z | Temp \= 35.00 °C | Flags \= 0
\>\>\> ALERT: Threshold exceeded \<\<\< <-- (POLLPRI received)
timestamp \= 2025-10-22T00:42:09.987Z | Temp \= 46.51 °C | Flags \= 1 <-- (Flag 1 \= New Sample \+ Threshold Crossed)

### **3.2 Test Mode (--test)**

This mode is used in the run\_demo.sh script for automated verification of the alert mechanism.

1. The script configures the threshold and period (if not specified, it uses the default value).
2. It monitors the poll() *loop* for a limited time (approximately **two sampling periods**).
3. If it detects POLLPRI, it prints TEST: PASS and exits with return code 0.
4. If the time expires without detecting the alert, it prints TEST: FAIL and exits with a non-zero return code.
