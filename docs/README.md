# **NXP Systems Software Engineer Candidate Challenge: Virtual Sensor (SimTemp)**

Este proyecto implementa un módulo de sensor de temperatura simulado fuera del árbol (*out-of-tree platform driver*) para el kernel de Linux. Expone lecturas periódicas a través de un dispositivo de caracteres (/dev/simtemp0) y permite la configuración y el monitoreo de alertas a través de SysFS.

## **1\. Estructura del Repositorio**

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
│  │  └─ main.py            \# CLI en Python (Consumidor de /dev/simtemp0)  
├─ scripts/  
│  ├─ build.sh           \# Compila el módulo .ko  
│  └─ run\_demo.sh        \# Insmod, Configura SysFS, Ejecuta Test, Rmmod  
├─ docs/  
│  ├─ README.md (Este archivo)  
│  ├─ DESIGN.md  
│  ├─ TESTPLAN.md  
│  ├─ CLI\_USAGE.md       \# Documentación de la CLI  
|  └─ AI_NOTES.md
|   
└─ .gitignore

## **2\. Requisitos Previos**

Necesitarás un sistema Linux (se recomienda Ubuntu LTS) con los encabezados del kernel instalados.

\# Ejemplo para Ubuntu  
sudo apt update  
sudo apt install build-essential python3 python3-pip  
sudo apt install linux-headers-$(uname \-r)

## **3\. Guía de Compilación**

Utiliza el script build.sh para compilar el módulo y verificar la sintaxis de la aplicación de usuario.

\# Desde el directorio raíz del proyecto  
./scripts/build.sh

Si la compilación es exitosa, se generará el archivo kernel/nxp\_simtemp.ko.

## **4\. Demostración y Ejecución (Demo)**

El script run\_demo.sh es la secuencia recomendada para probar el sistema completo:

1. Inserta el módulo nxp\_simtemp.ko.  
2. Espera la creación de los nodos /dev/simtemp0 y /sys/class/simtemp/simtemp0.  
3. Configura SysFS (Sampling: **2000ms**, Threshold: **20000 mC**).  
4. Ejecuta la CLI en **modo de prueba** (--test).  
5. La prueba finaliza si detecta la alerta (POLLPRI).  
6. Remueve el módulo.

\# Ejecutar la demo. Requiere permisos de root (usará sudo).  
./scripts/run\_demo.sh

## **5\. Pruebas Manuales y CLI**

### **5.1 Interacción SysFS**

Los parámetros clave del driver se configuran a través de SysFS. El dispositivo de ejemplo es simtemp0.

| Path | Descripción | Valores de Ejemplo |
| :---- | :---- | :---- |
| /sys/class/simtemp/simtemp0/sampling\_ms | Periodo de muestreo en milisegundos (RW). | echo 500 \> sampling\_ms |
| /sys/class/simtemp/simtemp0/threshold\_mc | Umbral de alerta en mili-°C (RW). | echo 42000 \> threshold\_mc |
| /sys/class/simtemp/simtemp0/mode | Modo de simulación (RW). | echo noisy \> mode |
| /sys/class/simtemp/simtemp0/stats | Contadores del driver (RO). | cat stats |

### **5.2 Uso de la CLI**

Para la lectura continua en tiempo real, use la aplicación de Python.

\# Leer lecturas continuas con un timeout de 1 segundo entre muestras  
python3 ./user/cli/main.py \--timeout 1 

Para más detalles sobre la CLI, consulte docs/CLI\_USAGE.md.

## **6\. Información de Envío (Commit Patch)**

Video Demo Link: https://youtu.be/seG8FFlLHk8
Git Repository Link: \[Enlace a mi repositorio público de GitHub/GitLab\]
