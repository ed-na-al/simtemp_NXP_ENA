# **Documentación: Aplicación de Línea de Comandos (CLI)**

El script main.py es la interfaz de usuario en Python diseñada para interactuar con el driver del kernel nxp\_simtemp de tres maneras principales:

1. **Lectura de Datos:** Lee y decodifica el stream binario de /dev/simtemp0.  
2. **Monitoreo de Alertas:** Utiliza poll() para esperar eventos de datos (POLLIN) y de alerta (POLLPRI).  
3. **Configuración:** Permite establecer el umbral y el periodo de muestreo a través de los archivos SysFS.

## **1\. Requisitos**

* Python 3.x  
* Permisos para leer/escribir SysFS (requiere sudo para escribir) y leer el nodo del dispositivo (/dev/simtemp0).

## **2\. Uso General**

El script soporta varios argumentos para controlar su comportamiento.

python3 ./user/cli/main.py \--help

### **Argumentos:**

| Argumento | Descripción | Tipo | Por Defecto |
| :---- | :---- | :---- | :---- |
| \-h, \--help | Muestra el mensaje de ayuda. | N/A | N/A |
| \-t, \--timeout | Tiempo de espera en segundos para poll() antes de reportar un *timeout*. | float | 10.0 |
| \-s, \--sampling | **OPCIONAL**. Configura el valor de sampling\_ms en SysFS antes de empezar a leer. Requiere permisos de root. | int (ms) | No configurado |
| \-d, \--threshold | **OPCIONAL**. Configura el valor de threshold\_mc en SysFS antes de empezar a leer. Requiere permisos de root. | int (mC) | No configurado |
| \--test | **Modo de Prueba**. Configura el umbral para forzar una alerta, espera un máximo de dos periodos de muestreo y retorna 0 si la alerta (POLLPRI) fue detectada. | flag | Desactivado |

## **3\. Modos de Operación**

### **3.1 Lectura Continua**

El modo por defecto simplemente espera datos y los imprime, mostrando tanto el *timestamp* del kernel como la temperatura decodificada.

\# Leer cada 5 segundos (asumiendo sampling\_ms=5000 en el driver)  
python3 ./user/cli/main.py  
\# Leer cada 500ms y cambiar el umbral a 40°C (40000 mC)  
sudo python3 ./user/cli/main.py \--sampling 500 \--threshold 40000

**Formato de Salida:**

timestamp \= 2025-10-22T00:41:06.661Z | Temp \= 35.00 °C | Flags \= 0 
\>\>\> ALERTA: Se superó el umbral \<\<\<  \<-- (POLLPRI recibido)  
timestamp \= 2025-10-22T00:42:09.987Z | Temp \= 46.51 °C | Flags \= 1  \<-- (Flag 1 \= Nuevo Sample \+ Umbral Cruzado)

### **3.2 Modo de Prueba (--test)**

Este modo se utiliza en el script run\_demo.sh para una verificación automatizada del mecanismo de alerta.

1. El script configura el umbral y el periodo (si no se especifica, usa el valor por defecto).  
2. Monitorea el *loop* de poll() por un tiempo limitado (aproximadamente **dos periodos de muestreo**).  
3. Si detecta POLLPRI, imprime TEST: PASS y sale con código de retorno 0\.  
4. Si el tiempo expira sin detectar la alerta, imprime TEST: FAIL y sale con un código de retorno no cero.


