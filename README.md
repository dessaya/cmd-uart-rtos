# cmd-uart

Trabajo práctico para las materias RTOS I y PCSE de la
[Carrera de Especialización en Sistemas Embebidos](http://laboratorios.fi.uba.ar/lse/especializacion.html)
de la UBA.

El firmware corre sobre la placa de desarrollo [EDU-CIAA-NXP](http://www.proyecto-ciaa.com.ar/index_comprar_educiaanxp.html),
y para compilar es necesario utilizar [Firmware v3](https://github.com/epernia/firmware_v3).

El objetivo del firmware es simular una línea de comandos simple mediante la
UART, corriendo en [FreeRTOS](https://www.freertos.org/). El funcionamiento se describe en el documento de
[propuesta de TP](https://docs.google.com/document/d/1LrijnJ_BgH8jhwjd66099hUURDXS5GVRP1gTCUJkr28/edit?usp=sharing).

La documentación se puede generar con el comando `doxygen Doxyfile`.

Los módulos (pares de archivos `.c` y `.h`) principales son:

* `terminal` controla la entrada/salida de texto mediante la UART.
* `cli` controla la línea de comandos.
* `commands` contiene la lista de comandos incluidos.
* `echo` implementa el comando `echo`.
* `sleep` implementa el comando `sleep`.
* `gpio` implementa el comando `gpio`, que permite leer/escribir en puertos
  GPIO.
* `i2c` implementa el comando `i2c`, que permite interactuar con cualquier
  dispositivo en el bus I2C.

## RTOS

Actualmente hay 3 tipos de tareas definidas, y sus prioridades están en el
archivo `task_priorities.h`. De mayor a menor prioridad:

* `gpio_blink_task`: El comando `gpio blink` lanza una instancia de esta
  tarea por cada puerto. Si el usuario modifica el periodo de blink de un
  puerto particular, se elimina su tarea y se lanza una nueva.
* `terminal_tx_task`: Se lanza al inicio. Controla la salida de la terminal,
  leyendo de una cola `txQueue`.
* `cli_task`: Es la tarea principal, que muestra la línea de comandos y ejecuta
  los comandos recibidos.

Además hay un ISR en el módulo `terminal` llamado `uart_rx_isr` que controla la
entrada de la UART y envía los datos recibidos a la cola `rxQueue`.
