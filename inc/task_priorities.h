#ifndef TASK_PRIORITIES_H
#define TASK_PRIORITIES_H

/** RTOS priority for the CLI task */
#define CLI_TASK_PRIORITY        tskIDLE_PRIORITY + 1
/** RTOS priority for all tasks spawned by the `irq` command. */
#define IRQ_TASK_PRIORITY        tskIDLE_PRIORITY + 2
/** RTOS priority for all tasks spawned by the `loop` command. */
#define LOOP_TASK_PRIORITY       tskIDLE_PRIORITY + 3
/** RTOS priority for the terminal output task. */
#define TERMINAL_TASK_PRIORITY   tskIDLE_PRIORITY + 4

#endif
