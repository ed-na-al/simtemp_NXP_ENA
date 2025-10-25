## **Plan de Pruebas de Alto Nivel (High-Level Test Plan)**

Este plan valida los principales criterios de aceptación del desafío del ingeniero de software.

### **T1 — Ciclo de Vida del Módulo (Load/Unload)**

| Objetivo | Descripción | Comandos | Criterios de Éxito |
| :---- | :---- | :---- | :---- |
| **T1.1** Build | Compilar el módulo y la aplicación de usuario. | ./scripts/build.sh | Compilación exitosa (nxp\_simtemp.ko generado). |
| **T1.2** Load | Insertar el módulo. | sudo insmod kernel/nxp\_simtemp.ko | No hay errores en dmesg. Los nodos /dev/simtemp0 y /sys/class/simtemp/simtemp0 existen. |
| **T1.3** SysFS Defaults | Verificar la lectura de los valores iniciales. | cat /sys/.../sampling\_ms | Los valores coinciden con los predeterminados (e.g., 1000\) o los definidos en el DT (e.g., 500). |
| **T1.4** Unload (Clean) | Remover el módulo. | sudo rmmod nxp\_simtemp | Módulo removido sin error "Device is busy". No hay advertencias/OOPS en dmesg. Los nodos se han ido. |

### **T2 — Ruta de Datos (Data Path) y Frecuencia**

| Objetivo | Descripción | Comandos | Criterios de Éxito |
| :---- | :---- | :---- | :---- |
| **T2.1** Lectura Binaria | Verificar que la CLI pueda leer un registro binario completo. | ./user/cli/main.py | La CLI decodifica correctamente 8 \+ 4 \+ 4 \= 16 bytes. Los campos timestamp\_ns y temp\_mC son válidos. |
| **T2.2** Frecuencia | Verificar que la frecuencia de muestreo se respeta. | ./user/cli/main.py \--sampling 200 | La salida muestra aproximadamente 5 lecturas por segundo. |
| **T2.3** Bloqueo | Verificar el bloqueo de read() y poll() sin O\_NONBLOCK. | dd if=/dev/simtemp0 bs=16 count=1 | dd se bloquea hasta que hay un nuevo sample. |

### **T3 — Umbral de Alerta y Eventos (POLLPRI)**

| Objetivo | Descripción | Comandos | Criterios de Éxito |
| :---- | :---- | :---- | :---- |
| **T3.1** Evento de Umbral | Configurar un umbral bajo para forzar una alerta. | echo 30000 \> /sys/.../threshold\_mc | La CLI (usando poll()) detecta un evento **POLLPRI**. El registro binario leído tiene el SIMTEMP\_FLAG\_THRESHOLD\_CROSSED (bit 1\) activado. |
| **T3.2** Modo Test | Ejecutar la prueba automática de la CLI. | ./scripts/run\_demo.sh | El script finaliza con un mensaje TEST: PASS y código de salida 0\. |
| **T3.3** Alerta Desactivada | Subir el umbral por encima de la temperatura simulada. | echo 60000 \> /sys/.../threshold\_mc | No se disparan eventos POLLPRI. |

### **T4 — Configuración SysFS**

| Objetivo | Descripción | Comandos | Criterios de Éxito |
| :---- | :---- | :---- | :---- |
| **T4.1** SysFS Write | Modificar sampling\_ms a un nuevo valor. | echo 50 \> /sys/.../sampling\_ms | El cambio se aplica y la frecuencia de muestreo aumenta inmediatamente. |
| **T4.2** Validación de Entrada | Intentar escribir un valor no válido. | echo "abc" \> /sys/.../sampling\_ms | La escritura falla y retorna \-EINVAL al usuario (el comando echo falla). El valor anterior se mantiene. |
| **T4.3** Stats | Verificar el incremento de contadores. | 1\. cat stats. 2\. Esperar 5s. 3\. cat stats. | total\_updates se incrementa en \~5x. total\_alerts se incrementa si el umbral está cruzado. |
| **T4.4** Mode | Cambiar el modo de simulación. | echo noisy \> /sys/.../mode | La temperatura simulada muestra más variación (ruido) en las lecturas de la CLI. |

### **T5 — Robustez (Concurrency & Error Paths)**

| Objetivo | Descripción | Criterios de Éxito |
| :---- | :---- | :---- |
| **T5.1** KFIFO Overflow | Intentar saturar el buffer (e.g., sampling\_ms=10). | cat stats |
| **T5.2** Unload Under Load | Remover el módulo mientras la CLI está leyendo. | sudo rmmod mientras la CLI se ejecuta. |

