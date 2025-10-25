# **7\) Problem-Solving Write-Up: Diseño y Arquitectura del Driver SimTemp**

Este documento describe el diseño, la interacción entre módulos, las decisiones de sincronización y el análisis de escalabilidad para el driver de sensor virtual nxp\_simtemp.

## **7.1. Interacción Kernel y Userspace**

### **Diagrama de Bloques (Conceptual)**

El sistema opera a través de una arquitectura cliente-servidor, donde el Módulo del Kernel actúa como productor de datos y el Userspace CLI como consumidor, mediado por interfaces estándar de Linux.

**Flujo de Comunicación Clave:**

1. **Userspace (CLI)** $\\rightarrow$ **Kernel (Configuración SysFS):**  
   * **Propósito:** Establecer parámetros de operación.  
   * **Mecanismo:** La CLI escribe valores (e.g., 2000\) en archivos SysFS (e.g., /sys/class/simtemp/simtemp0/sampling\_ms).  
   * **Kernel:** Las funciones store asociadas a estos archivos actualizan las variables internas protegidas por un spinlock.  
2. **Kernel (Temporización** $\\rightarrow$ **Datos KFIFO):**  
   * **Propósito:** Generación periódica de datos.  
   * **Mecanismo:** Una tarea retrasada (delayed\_work) se programa para ejecutarse cada *N* milisegundos (sampling\_ms).  
   * **Kernel:** La tarea genera un nuevo *sample*, lo inserta atómicamente en el **KFIFO**, y despierta (wake\_up\_interruptible) la cola de espera de lectura (read\_wq).  
3. **Userspace (CLI)** $\\rightarrow$ **Kernel (Lectura de Datos):**  
   * **Propósito:** Consumir el flujo de *samples*.  
   * **Mecanismo:** La CLI llama a read() sobre /dev/simtemp0.  
   * **Kernel:** Si el KFIFO tiene datos, se transfiere un registro de 16 bytes a *userspace*. Si está vacío, la lectura se bloquea en la read\_wq hasta que la workqueue despierte a la cola.  
4. **Kernel (Alerta)** $\\rightarrow$ **Userspace (Notificación Poll):**  
   * **Propósito:** Notificar un evento de alta prioridad.  
   * **Mecanismo:** Si el nuevo *sample* excede el threshold\_mc, el kernel notifica una condición **POLLPRI** a través del *file descriptor* de /dev/simtemp0.  
   * **Userspace:** La CLI que llama a poll() detecta este evento inmediatamente sin necesidad de leer el flujo de datos principal.

## **7.2. Decisiones de Diseño Clave**

### **A. Locking Choices (Spinlocks vs. Mutexes)**

Se han elegido **spinlocks** para proteger los recursos críticos del driver, ya que el *driver* es un componente de tiempo crítico y evita dormir (sleep) en contextos donde es vital la rapidez (como interrupciones o *workqueues*).

| Recurso Protegido | Mecanismo de Locking | Uso / Razón | Código (Referencia) |
| :---- | :---- | :---- | :---- |
| **KFIFO** (simtemp\_device.fifo) | **Spinlock** (simtemp\_device.fifo\_lock) | El KFIFO es manipulado en dos contextos diferentes: la *workqueue* (productor) y el read() (consumidor). Dado que la manipulación es rápida (un kfifo\_put o kfifo\_get), un *spinlock* es la opción más ligera para garantizar la atomicidad y evitar que la *workqueue* se duerma. | simtemp\_worker\_func, simtemp\_read |
| **State / Config** (sampling\_ms, threshold\_mc, mode) | **Spinlock** (simtemp\_device.state\_lock) | La configuración puede ser modificada por *userspace* a través de SysFS (store methods) y leída por el *workqueue*. Un *spinlock* asegura que las operaciones de lectura/escritura en estas variables compartidas sean atómicas, protegiendo contra condiciones de carrera entre *userspace* (SysFS) y la *workqueue* periódica. | simtemp\_show/store, simtemp\_worker\_func |

### **B. API Trade-offs**

| Función | Mecanismo Elegido | Razón del Trade-Off |
| :---- | :---- | :---- |
| **Configuración** (sampling\_ms, threshold\_mc, mode) | **SysFS** | Es el mecanismo estándar y preferido del kernel para la configuración de un solo valor por dispositivo. Es simple, visible en el sistema de archivos. |
| **Lectura de Datos** (Stream de samples) | **Device File (/dev/simtemp0)** | **read()** es ideal para flujos de datos periódicos. Es simple y permite el bloqueo/no bloqueo (O\_NONBLOCK). |
| **Notificación de Eventos** (Alerta de Umbral) | **poll() / POLLPRI** | poll() es el mecanismo canónico para notificar eventos asíncronos en dispositivos de caracteres (junto con select y epoll). El uso de POLLPRI (alerta de prioridad) lo diferencia claramente de una simple llegada de datos (POLLIN). Esto permite a *userspace* reaccionar inmediatamente a la alerta sin tener que leer y decodificar el registro binario completo. |

### **C. Device Tree Mapping**

El código se intentó con Device  Tree pero surgió que la plataforma no era compatible, por lo tanto, no se pudo probar adecuadamente este punto. 

### **D. Scaling (¿Qué se rompe a 10 kHz?)**

A una frecuencia de muestreo de **10 kHz (100** $\\mu$**s)**, el sistema intenta generar un *sample* binario de 16 bytes cada 100 $\\mu$s.

| Componente | Problema de Escalabilidad | Estrategia de Mitigación |
| :---- | :---- | :---- |
| **delayed\_work** | **Alto consumo de CPU (Soft IRQ):** La tarea se ejecuta 10,000 veces por segundo. La sobrecarga de programar y ejecutar la tarea retrasa puede consumir una parte significativa de la CPU, especialmente en sistemas de un solo núcleo. | Minimiza la latencia de programación de tareas, pero la función de *callback* debe ser lo más corta y rápida posible. |
| **KFIFO (Tasa de Producción)** | **Latencia y Overheads de Locking:** El *spinlock* se adquiere y libera 10,000 veces por segundo. Si bien es rápido, esta tasa de contención podría convertirse en un cuello de botella. Además, el *userspace* tiene solo 100 $\\mu$s para leer el dato antes de que llegue el siguiente (si SAMPLE\_FIFO\_SIZE es 1). | **Aumentar el Tamaño del KFIFO:** Definir SAMPLE\_FIFO\_SIZE a un valor mucho mayor (e.g., 256 o 512\) y permitir que la *workqueue* se ejecute solo cuando el FIFO esté medio lleno. Esto amortigua el costo del I/O y el locking. |
| **Userspace (read()/poll())** | **Consumo de CPU en read():** El *userspace* tendría que estar leyendo continuamente 10,000 veces por segundo para evitar el *overflow* del KFIFO. | **Bloqueo Inteligente/Batching:** La CLI debería leer datos en *batches* (read() de múltiples samples) o el tamaño del KFIFO debe ser grande para que el *userspace* pueda tolerar ráfagas de alta frecuencia. |

En resumen, el principal problema es el **costo de programar y ejecutar la tarea 10,000 veces por segundo**. La solución requiere migrar un temporizador que puede con la tarea, Para una temporización de baja latencia junto con un **KFIFO de mayor tamaño** para permitir la lectura en *batches*.

### **7.3 Futuras Mejoras**

* Se puede mencionar que el uso de la simulación de un sensor de temperatura puede darse una idea de para qué podría funcionar. Si es para uso industrial y se require un seguimiento constante y con un background para futuros analisis el hecho de crear una base de datos para futuros analisis y decisiones de optimización de equipo.

* Otro proceso es la generalización mayor del driver, ya que es un sólo dispositivo, el driver en su mayoría trabaja estaticamente, por lo tanto, no se puede escalar tan facilmente, así que se requiere una estructura más dinámica y escalable

* Aplicar correctamente un DT Binding en el driver para una estrucutura igualemnte escalable y modularizada. 
