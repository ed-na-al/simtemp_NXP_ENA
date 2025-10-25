# **AI NOTES**

En este AI_NOTES se describe cómo se usó la inteligencia artificial para el desarrollo del proyecto. Al no saber nada sobre desarrollo de drivers, el kernel de linux, y un conocimiento intermedio de lenguaje C y Python, bajo en bash. Ayudó a consolidar más la información sabida y además a ampliar la velocidad de trabajo, ayudando en comprender mejor los conceptos que ya se entendía gracias a los libros leídos. 

## **1\. Primer Chat: Ruta de trabajo**

In this chat, I received guidance and technical support for preparing a Linux kernel driver project as part of a technical interview challenge. The discussion focused on understanding the project requirements, clarifying kernel-level concepts such as platform drivers, character devices, sysfs interfaces, and concurrency handling. I also received advice on documentation practices and project structure to ensure clarity, modularity, and maintainability. The goal was to build and document a clean, functional simulation driver that demonstrates good design, code organization, and understanding of kernel-user space interaction.

Y además se establecierón fuentes confiables de información para el desarrollo de drivers como son los libros:
1. 
2. 
3. 

## **2\. Segundo Chat: Dudas Conceptos Kernel/Drivers**

En este chat estuve revisando y aclarando conceptos relacionados con el Linux Device Model, en especial lo que describe los capítulos 1, 2, 3, 6 y 14 de Linux Device Drivers, además de conceptos básicos de desarrollo en el Kernel (como pueden ser los métodos de sincronización, interrupciones, concurrencia, etc.). Analicé cómo el kernel organiza los dispositivos, drivers y buses mediante estructuras como kobject, kset y class, así como su representación jerárquica en el sistema de archivos virtual /sys. También revisé cómo se pueden crear dispositivos simulados y exponer sus atributos al espacio de usuario, y cuándo es necesario o no registrar un driver dentro del modelo. En general, fue una conversación técnica enfocada en entender la estructura y funcionamiento interno del modelo de dispositivos en Linux.

## **3\. Tercer Chat: Dudas C Kernel**

Durante esta conversación revisé y consolidé varios conceptos fundamentales de programación en C y de desarrollo en kernel de Linux. Principalmente, trabajé en comprender el manejo de punteros —incluyendo punteros simples, dobles punteros y su relación con estructuras—, así como la diferencia entre memoria en el stack y el heap. También reforcé cómo funcionan funciones como malloc y su uso correcto para la asignación dinámica de memoria.

Además, revisé cómo las estructuras pueden contener punteros y cómo se interpretan cuando apuntan a otros tipos o a listas de estructuras, utilizando analogías y ejemplos simples para entender su uso en contextos reales, como el kernel. Finalmente, repasé la lógica detrás de los punteros en funciones, el paso de direcciones de memoria, el retorno de punteros y la interpretación de campos con doble puntero dentro de estructuras del kernel.

## **4\. Cuarto Chat: Dudas Códigos de ejemplo**

En este chat estuvimos revisando conceptos fundamentales del desarrollo de drivers en Linux, basándonos en ejemplos del libro Linux Device Drivers (LDD) y otros recursos del kernel. Analizamos cómo funciona la relación entre el espacio de usuario y el espacio del kernel, el papel de estructuras como file, inode, cdev y file_operations, y cómo se enlazan para permitir la comunicación entre el dispositivo y el sistema operativo.

También se revisó el flujo de apertura y manejo de dispositivos, el uso de macros como __init y __exit, el registro dinámico de números mayor y menor, y la interacción del driver con el sistema de archivos a través de sysfs. Finalmente, se profundizó en cómo los atributos de clase y de dispositivo permiten exponer información o parámetros configurables al usuario desde el kernel, explicando la diferencia entre class_create_file() y device_create_file().

## **5\. Quinto Chat: Dudas Códigos de prueba**

En este chat estuvimos revisando conceptos fundamentales del desarrollo de drivers en Linux, basándonos en ejemplos del libro Linux Device Drivers (LDD) y otros recursos del kernel. Analizamos cómo funciona la relación entre el espacio de usuario y el espacio del kernel, el papel de estructuras como file, inode, cdev y file_operations, y cómo se enlazan para permitir la comunicación entre el dispositivo y el sistema operativo.

También se revisó el flujo de apertura y manejo de dispositivos, el uso de macros como __init y __exit, el registro dinámico de números mayor y menor, y la interacción del driver con el sistema de archivos a través de sysfs. Finalmente, se profundizó en cómo los atributos de clase y de dispositivo permiten exponer información o parámetros configurables al usuario desde el kernel, explicando la diferencia entre class_create_file() y device_create_file().

## **Comprobación**

La principal forma de comprobación de la información fue técnica, ya que como era código la ejecución de este mismo se observaba el comportamiento descrito por la IA. El enfoque que fue por conceptos, teoría y aprendizaje sobre estos, fue realizado por medio de los libros descritos en el **punto 1**, ya que eran fuentes confiables y describían los conceptos adecuadamente, solo era cuestión de usar la IA para comprender mejor los conceptos para evitar un entendimiento de la información incorrecta.
