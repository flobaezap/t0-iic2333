## Uso de la Shell (Comandos soportados hasta ahora)

Hasta ahora, `Burnssh` soporta los siguientes comandos:

* **`launch <ejecutable> [argumentos...]`**: Lanza un nuevo proceso en segundo plano. Por ejemplo, `launch sleep 10` o `launch ls -l`. La shell no se queda pegada esperando a que termine, sino que te deja seguir escribiendo.
* **`status`**: imprime el historial de todos los procesos que han sido lanzados desde que se abrió la shell. Muestra su PID, nombre, tiempo de ejecución, si están pausados o no, su Exit Code y el Signal Value.
* **`exit`**: comando temporal para cerrar la shell rápidamente, si no igual sirve ctrl+C.

## Funciones Principales

Se definió un arreglo de structs llamado `process_table` en el que se guarda toda la info actualizada de cada proceso que ha sido ejecutado en la shell.

Las funciones implementadas hasta ahora:

### 1. `update_finished_processes()`
Sirve para revisar que no hayan procesos zombies ni huérfanos básicamente. Se ejecuta al principio de cada ciclo de la shell y usa la syscall `waitpid` con la opción `WNOHANG`. Esto revisa rápidamente si algún proceso hijo ya terminó, sin bloquear la consola. Si encuentra a uno que ya terminó, actualiza su estado en la tabla calculando el tiempo total y guardando el código de salida o la señal que lo mató.

### 2. `register_new_process(pid_t pid, char* executable)`
Esta función es la responsable de guardar los procesos nuevos en la tabla de procesos. Registra toda la info de un proceso que nos solicitan para que luego se pueda actualizar o mostrar en el status.

### 3. `execute_launch(char** input)`
Usa dos syscalls, fork y execvp basándose mucho en el código simplificado de una shell mostrado en clases.
* Al hijo le dice que reemplace su código por el ejecutable ingresado. Si el ejecutable no existe, se avisa del error y el hijo muere con `exit(1)`.
* Al padre le pide que llame a `register_new_process()` para guardar el historial de los procesos.

### 4. `execute_status(char** input)`
Lee la tabla de procesos e imprime el listado con un formato ordenado (se formateó con Gemini). Si un proceso sigue corriendo, calcula su tiempo de ejecución en vivo restando la hora actual con la hora en que empezó.

