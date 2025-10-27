# **NXP Systems Software Engineer Candidate Challenge: Virtual Sensor (SimTemp)**

This project implements an out-of-tree platform driver simulated temperature sensor module for the Linux kernel. It exposes periodic readings through a character device (/dev/simtemp0) and allows configuration and alert monitoring via SysFS.

## **1\. Repository Structure**

simtemp/
├─ kernel/
│  ├─ Kbuild
│  ├─ Makefile
│  ├─ nxp\_simtemp.c
│  ├─ nxp\_simtemp.h
│  └─ dts/
│     └─ nxp-simtemp.dtsi
├─ user/
│  ├─ cli/
│  │  └─ main.py            # Python CLI (Consumer of /dev/simtemp0)
├─ scripts/
│  ├─ build.sh           # Compiles the .ko module
│  └─ run\_demo.sh        # Insmod, SysFS Configuration, Run Test, Rmmod
├─ docs/
│  ├─ README.md (This file)
│  ├─ DESIGN.md
│  ├─ TESTPLAN.md
│  ├─ CLI\_USAGE.md       # CLI Documentation
|  └─ AI_NOTES.md
|   
└─ .gitignore

## **2\. Prerequisites**

You will need a Linux system (Ubuntu LTS is recommended) with the kernel headers installed.

# Example for Ubuntu
sudo apt update
sudo apt install build-essential python3 python3-pip
sudo apt install linux-headers-$(uname \-r)

## **3\. Compilation Guide**

Use the build.sh script to compile the module and verify the syntax of the user application.

# From the project root directory
./scripts/build.sh

If compilation is successful, the kernel/nxp\_simtemp.ko file will be generated.

## **4\. Demonstration and Execution (Demo)**

The run\_demo.sh script is the recommended sequence for testing the complete system:

1. Insert the nxp\_simtemp.ko module.
2. Wait for the creation of the /dev/simtemp0 and /sys/class/simtemp/simtemp0 nodes.
3. Configure SysFS (Sampling: **2000ms**, Threshold: **20000 mC**).
4. Run the CLI in **test mode** (--test).
5. The test concludes if the alert is detected (POLLPRI).
6. Remove the module.

# Run the demo. Requires root permissions (will use sudo).
./scripts/run\_demo.sh

## **5\. Manual Testing and CLI**

### **5.1 SysFS Interaction**

The driver's key parameters are configured via SysFS. The example device is simtemp0.

| Path | Description | Example Values |
| :---- | :---- | :---- |
| /sys/class/simtemp/simtemp0/sampling\_ms | Sampling period in milliseconds (RW). | echo 500 \> sampling\_ms |
| /sys/class/simtemp/simtemp0/threshold\_mc | Alert threshold in milli-°C (RW). | echo 42000 \> threshold\_mc |
| /sys/class/simtemp/simtemp0/mode | Simulation mode (RW). | echo noisy \> mode |
| /sys/class/simtemp/simtemp0/stats | Driver counters (RO). | cat stats |

### **5.2 CLI Usage**

For continuous real-time reading, use the Python application.

# Read continuous readings with a 1-second timeout between samples
python3 ./user/cli/main.py \--timeout 1 

For more details on the CLI, consult docs/CLI\_USAGE.md.

## **6\. Submission Information (Commit Patch)**

Video Demo Link: https://youtu.be/seG8FFlLHk8
Git Repository Link: \[Link to my public GitHub/GitLab repository\]
