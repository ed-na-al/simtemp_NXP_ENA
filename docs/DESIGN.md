# **7\) Problem-Solving Write-Up: SimTemp Driver Design and Architecture**

This document describes the design, interaction between modules, synchronization decisions, and scalability analysis for the nxp\_simtemp virtual sensor driver.

## **7.1. Kernel and Userspace Interaction**

### **Block Diagram (Conceptual)**

The system operates through a client-server architecture, where the Kernel Module acts as the data producer and the Userspace CLI as the consumer, mediated by standard Linux interfaces.

**Key Communication Flow:**

1. **Userspace (CLI)** $\rightarrow$ **Kernel (SysFS Configuration):**
   * **Purpose:** Set operational parameters.
   * **Mechanism:** The CLI writes values (e.g., 2000) to SysFS files (e.g., /sys/class/simtemp/simtemp0/sampling\_ms).
   * **Kernel:** The store functions associated with these files update internal variables protected by a spinlock.
2. **Kernel (Timing** $\rightarrow$ **KFIFO Data):**
   * **Purpose:** Periodic data generation.
   * **Mechanism:** A delayed task (delayed\_work) is scheduled to run every *N* milliseconds (sampling\_ms).
   * **Kernel:** The task generates a new *sample*, atomically inserts it into the **KFIFO**, and wakes up (wake\_up\_interruptible) the read wait queue (read\_wq).
3. **Userspace (CLI)** $\rightarrow$ **Kernel (Data Reading):**
   * **Purpose:** Consume the stream of *samples*.
   * **Mechanism:** The CLI calls read() on /dev/simtemp0.
   * **Kernel:** If the KFIFO has data, a 16-byte record is transferred to *userspace*. If it is empty, the read blocks on the read\_wq until the workqueue wakes up the queue.
4. **Kernel (Alert)** $\rightarrow$ **Userspace (Poll Notification):**
   * **Purpose:** Notify a high-priority event.
   * **Mechanism:** If the new *sample* exceeds the threshold\_mc, the kernel notifies a **POLLPRI** condition via the *file descriptor* for /dev/simtemp0.
   * **Userspace:** The CLI calling poll() detects this event immediately without needing to read the main data stream.

## **7.2. Key Design Decisions**

### **A. Locking Choices (Spinlocks vs. Mutexes)**

**Spinlocks** have been chosen to protect the driver's critical resources, as the *driver* is a time-critical component and avoids sleeping in contexts where speed is vital (such as interrupts or *workqueues*).

| Protected Resource | Locking Mechanism | Usage / Reason | Code (Reference) |
| :---- | :---- | :---- | :---- |
| **KFIFO** (simtemp\_device.fifo) | **Spinlock** (simtemp\_device.fifo\_lock) | The KFIFO is manipulated in two different contexts: the *workqueue* (producer) and the read() (consumer). Since manipulation is fast (a kfifo\_put or kfifo\_get), a *spinlock* is the lightest option to ensure atomicity and prevent the *workqueue* from sleeping. | simtemp\_worker\_func, simtemp\_read |
| **State / Config** (sampling\_ms, threshold\_mc, mode) | **Spinlock** (simtemp\_device.state\_lock) | Configuration can be modified by *userspace* via SysFS (store methods) and read by the *workqueue*. A *spinlock* ensures that read/write operations on these shared variables are atomic, protecting against race conditions between *userspace* (SysFS) and the periodic *workqueue*. | simtemp\_show/store, simtemp\_worker\_func |

### **B. API Trade-offs**

| Feature | Chosen Mechanism | Trade-Off Reason |
| :---- | :---- | :---- |
| **Configuration** (sampling\_ms, threshold\_mc, mode) | **SysFS** | This is the standard and preferred kernel mechanism for single-value configuration per device. It is simple, visible in the file system. |
| **Data Reading** (Stream of samples) | **Device File (/dev/simtemp0)** | **read()** is ideal for periodic data streams. It is simple and allows blocking/non-blocking (O\_NONBLOCK). |
| **Event Notification** (Threshold Alert) | **poll() / POLLPRI** | poll() is the canonical mechanism for notifying asynchronous events on character devices (along with select and epoll). Using POLLPRI (priority alert) clearly differentiates it from a simple data arrival (POLLIN). This allows *userspace* to react immediately to the alert without having to read and decode the full binary record. |

### **C. Device Tree Mapping**

The code was attempted with Device Tree but it was found that the platform was not compatible, therefore this point could not be tested properly.

### **D. Scaling (What breaks at 10 kHz?)**

At a sampling frequency of **10 kHz (100** $\\mu$**s)**, the system attempts to generate a 16-byte binary *sample* every 100 $\\mu$s.

| Component | Scalability Issue | Mitigation Strategy |
| :---- | :---- | :---- |
| **delayed\_work** | **High CPU Consumption (Soft IRQ):** The task runs 10,000 times per second. The overhead of scheduling and executing the delayed task can consume a significant portion of the CPU, especially on single-core systems. | Minimize task scheduling latency, but the *callback* function must be as short and fast as possible. |
| **KFIFO (Production Rate)** | **Latency and Locking Overheads:** The *spinlock* is acquired and released 10,000 times per second. While fast, this contention rate could become a bottleneck. Furthermore, *userspace* only has 100 $\\mu$s to read the data before the next one arrives (if SAMPLE\_FIFO\_SIZE is 1). | **Increase KFIFO Size:** Define SAMPLE\_FIFO\_SIZE to a much larger value (e.g., 256 or 512) and allow the *workqueue* to run only when the FIFO is half full. This amortizes the cost of I/O and locking. |
| **Userspace (read()/poll())** | **CPU Consumption in read():** *Userspace* would have to be reading continuously 10,000 times per second to prevent KFIFO *overflow*. | **Intelligent Blocking/Batching:** The CLI should read data in *batches* (read() of multiple samples) or the KFIFO size must be large so that *userspace* can tolerate high-frequency bursts. |

In summary, the main problem is the **cost of scheduling and executing the task 10,000 times per second**. The solution requires migrating to a timer that can handle the task, along with a **larger KFIFO size** to allow for *batch* reading.

### **7.3 Future Improvements**

* It can be mentioned that using a temperature sensor simulation can give an idea of what it could be used for. If it is for industrial use and constant monitoring is required, and with a background for future analysis, the fact of creating a database for future analysis and equipment optimization decisions.

* Another process is the greater generalization of the driver, since it is a single device, the driver mostly works statically, therefore, it cannot be scaled so easily, so a more dynamic and scalable structure is required.

* Correctly apply a DT Binding in the driver for an equally scalable and modularized structure.
