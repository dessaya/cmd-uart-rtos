# cmd-uart

Trabajo práctico para las materias RTOS I y PCSE de la
[Carrera de Especialización en Sistemas Embebidos](http://laboratorios.fi.uba.ar/lse/especializacion.html)
de la UBA.

El firmware corre sobre la placa de desarrollo [EDU-CIAA-NXP](http://www.proyecto-ciaa.com.ar/index_comprar_educiaanxp.html),
y para compilar es necesario utilizar [Firmware v3](https://github.com/epernia/firmware_v3).

El objetivo del firmware es simular una línea de comandos simple mediante la
UART, corriendo en [FreeRTOS](https://www.freertos.org/). El funcionamiento
planeado originalmente se describe en el documento de
[propuesta de TP](https://docs.google.com/document/d/1LrijnJ_BgH8jhwjd66099hUURDXS5GVRP1gTCUJkr28/edit?usp=sharing).

La documentación se puede generar con el comando `doxygen Doxyfile`.

Los módulos (pares de archivos `.c` y `.h`) principales son:

* `terminal.c` controla la entrada/salida de texto mediante la UART.
* `cli.c` controla la línea de comandos.
* `commands.c` contiene la lista de comandos incluidos.
* `echo.c` implementa el comando `echo`.
* `sleep.c` implementa el comando `sleep`.
* `loop.c` implementa el comando `loop`, que permite lanzar una tarea que ejecuta
  otro comando en un ciclo infinito.
* `gpio.c` implementa el comando `gpio`, que permite leer/escribir en puertos
  GPIO.
* `irq.c` implementa el comando `irq`, que permite ejecutar un comando
  arbitrario cuando un GPIO lanza una interrupción.
* `i2c.c` implementa el comando `i2c`, que permite interactuar con cualquier
  dispositivo en el bus I2C.

## RTOS

Hay 4 tipos de tareas definidas, y sus prioridades están en el
archivo `task_priorities.h`. De mayor a menor prioridad:

* `terminal_tx_task`: Se lanza al inicio. Controla la salida de la terminal,
  leyendo de una cola `txQueue`.
* `loop_task`: Puede haber hasta 4 instancias. Se lanza una cada vez
  que se ejecuta el comando `loop start`.
* `irq_subcommand_task`: Puede haber hasta 4 instancias. Se lanza una con el
  comando `irq`.
* `cli_task`: Es la tarea principal, que muestra la línea de comandos y ejecuta
  los comandos recibidos.

Además hay dos manejadores de interrupcion:

* Un ISR en el módulo `terminal` llamado `uart_rx_isr` que controla la
  entrada de la UART y envía los datos recibidos a la cola `rxQueue`.
* Cuatro ISRs en el módulo `irq` (`GPIO<n>_IRQHandler`), que se ejecutan mediante los puertos GPIO.

![Diagrama de componentes RTOS](/rtos.svg?raw=true)
