## Implementación T0

### Librerías Principales

* **`<unistd.h>`**: Utilizada para las llamadas al sistema fundamentales de creación y reemplazo de procesos (`fork` y `execvp`).
* **`<sys/wait.h>`**: Utilizada para la función `waitpid`, esencial para la recolección de procesos finalizados (zombies).
* **`<signal.h>`**: Necesaria para el manejo de señales asíncronas mediante la función `kill` (`SIGTERM`, `SIGINT`, `SIGKILL`).

---

### Estructuras y Variables Globales

* **`struct Process`**: Define la estructura para cada proceso, almacenando su PID, nombre del ejecutable, tiempos de ejecución, si está pausado, si está vivo, el estado de salida (`exit_code` y `signal_value`), momento en que se inicio una pausa (solo guarda el valor del último pause hecho), tiempo total de pausa (considera todas las pausas), si recibió una señal SIGTERM y el momento en el que se recibió la señal SIGTERM.
* **`process_table`**: Arreglo de tamaño fijo (100 elementos) que funciona como una tabla que almacena todos los procesos que han sido ejecutados en la shell, guardando el registro de toda la información de estos.
* **Variables de Control**:
  * `shutdown_active`: flag (0 o 1) que indica si el proceso de shutdown de la shell está activo.
  * `shutdown_start_time`: Registra la hora exacta en la que inició la cuenta regresiva del shutdown.
  * `pending_aborts`: Arreglo que guarda los PIDs de los procesos que iban a ejecutar un abort, permitiendo anularlos si es necesario.
  * `pending_aborts_count`: contador de procesos abort que están pendientes.
  * `time_max`: nos indica cual es el tiempo máximo de ejecución que los procesos deben tener. Si no se entrega un valor al inicializar la shell, el valor seteado es -1 e indica que los procesos tienen tiempo ilimitado de ejecución.

---

### Descripción de Funciones

* **`imprimir_proceso`**: función encargada de formatear la salida en pantalla. Si el proceso sigue en ejecución (`is_alive`), calcula su tiempo en vivo restando la hora actual (`time(NULL)`) con su `start_time`. Si ya finalizó, utiliza el `elapsed_time` guardado. Traduce el estado de pausa a texto ("True" o "False") e imprime los datos según el formato exigido.

* **`actualizar_procesos`**: función llamada al inicio de cada ciclo de la shell. Utiliza `waitpid` con la flag `WNOHANG` (Wait, No Hang), lo que permite revisar si algún proceso hijo ha terminado sin bloquear la consola. Si ve que hay un proceso finalizado, toma su PID, lo marca como inactivo (`is_alive = 0`) en la `process_table` y congela su tiempo total. Dependiendo de cómo murió, extrae su Exit Code(`WIFEXITED`) o la señal que lo forzó a terminar (`WIFSIGNALED`), y finalmente avisa al usuario por pantalla.

* **`registrar_proceso`**: esta función toma la primera fila disponible en la `process_table` y la inicializa con los datos del nuevo proceso recién creado. Copia el nombre de forma segura (`strncpy`), anota la hora de inicio y deja los valores del Exit Code y de Signal Value por defecto en `-1`, además de dejar el tiempo en que se pausa el proceso, el tiempo total de pausa del proceso, la flag que nos muestra si el proceso recibió o no una señal SIGTERM y el tiempo desde que fue recibida todo por defecto en `0`. Finalmente, incrementa el contador `process_count` para que el siguiente proceso se pueda registrar en la fila siguiente.

* **`ejecutar_launch`**: esta función verifica que el usuario haya ingresado un ejecutable válido. Luego, utiliza `fork()` para dividir en dos el programa. El proceso hijo ejecuta `execvp` para reemplazar su imagen de memoria por la del programa solicitado (o se suicida con `exit(1)` si el comando no existe). Por su parte, el proceso padre continúa su ejecución, avisa que el proceso fue lanzado y llama a la función `registrar_proceso`.

* **`ejecutar_status`**: Revisa si existen procesos en la `process_table`. Si es que existen, recorre la `process_table` y llama a la función `imprimir_proceso` para mostrar el estado actual de cada uno.

* **`ejecutar_abort`**: Convierte el tiempo ingresado como argumento a entero e instancia el arreglo `pids_to_abort` que tendrá los PIDs de los procesos que están corriendo en ese exacto momento, ignorando los que se inicien después. Luego, para evitar que se congele la shell delega la espera creando un proceso hijo con `fork()`. Este hijo duerme (`sleep`) durante el tiempo indicado en el argumento de la función. Al despertar, verifica si los procesos en `pids_to_abort` siguen vivos utilizando `kill(pid, 0)`. Si siguen activos, imprime su información y los termina enviando la señal `SIGTERM`. El proceso padre, por otro lado, anota el PID del hijo creado en la lista `pending_aborts` por si requiere ser anulado en caso de que se ejecute un shutdown en la shell.

* **`ejecutar_pause`**: Busca en la `process_table` el PID ingresado por el usuario para verificar si existe un proceso hijo en ejecución con ese identificador. Si lo encuentra, actualiza el valor de la variable `encontrado` a `1`, y si ya se encuentra pausado, avisa al usuario. Si está corriendo normalmente, le envía la señal `SIGSTOP` para detener su ejecución, actualiza su estado indicando que está pausado (`is_paused = 1`) y guarda la hora exacta en la que se detuvo (`pause_start_time`). Esto último es importante para calcular el tiempo de ejecución. Si no encuentra el proceso, indica que el PID no es válido.

* **`ejecutar_resume`**: Busca en la `process_table` el PID ingresado por el usuario. Si el proceso existe y efectivamente se encuentra pausado, le envía la señal `SIGCONT` para que retome su ejecución, además de actualizar el valor de la variable `encontrado` a `1`. Luego, actualiza su estado (`is_paused = 0`) y calcula exactamente cuánto tiempo estuvo dormido (restando la hora actual con el `pause_start_time` guardado previamente). Este tiempo de pausa se suma a la variable `total_paused_time` para que, al momento de imprimir el `status`, el programa descuente los segundos inactivos y muestre solo el tiempo real de ejecución. Si no encuentra el proceso o este no estaba pausado, avisa que el PID no es válido.

* **`ejecutar_shutdown`**: Inicia el protocolo de cierre. Primero, recorre `pending_aborts` y elimina con `SIGKILL` cualquier comando abort programado que estuviera en espera. Luego, envía la señal (`SIGINT`) a todos los procesos activos, dándoles la oportunidad de parar lo que estaban haciendo y terminar ordenadamente durante un tiempo de 10 segundos. Actualiza `shutdown_active = 1` y registra la hora de inicio. Si no había procesos corriendo, imprime la `process_table` y cierra la shell de inmediato.

* **`revisar_tiempo_maximo`**: se encarga de vigilar que los procesos no excedan el límite de ejecución definido al correr la shell. Si el límite existe (`time_max != -1`), recorre la `process_table` analizando únicamente a los procesos que siguen activos. Para cada uno, calcula su tiempo de ejecución real sin contar las pausas. Si este tiempo alcanza o supera el máximo, y el proceso no ha recibido la señal de advertencia previamente, le envía la señal `SIGTERM`, actualiza la flag `timeout_sigterm_sent = 1` y guarda la hora exacta del aviso. En iteraciones posteriores, si el proceso ya había recibido la señal `SIGTERM`, revisa si transcurrieron los 5 segundos del "tiempo de gracia", si es así, le envía la señal letal `SIGKILL` para forzar su termino y actualiza la flag a 2 para no repetir el envío.

* **`main`**: primero configura el buffer de entrada, revisa si se le entregó el argumento de tiempo máximo de ejecución, si es que se entrega un valor se asigna ese valor como int a la variable global `time_max`. Luego, inicia un bucle infinito `while(TRUE)`. En cada vuelta:
  1. Llama a `actualizar_procesos` para detectar a los procesos zombies, limpiarlos y actualizar la tabla.
  2. Llama a `revisar_tiempo_maximo`, de tal forma que se asegura que el tiempo de ejecución de los procesos vivos es menor al tiempo máximo habilitado al inicializar la shell. Si no hay tiempo máximo (`time_max = -1`), la función llama a `return` y seguimos con la ejecución del bloque `while(TRUE)`.
  3. Verifica si la cuenta regresiva de `shutdown_start_time` superó los 10 segundos. Si es así, manda la señal `SIGKILL` a los procesos que sobrevivieron, hace una última actualización con `actualizar_procesos`, imprime la tabla final y rompe el ciclo para terminar.
  4. Si `shutdown_active = 0`, lee el input de la shell. Compara el input con los comandos existentes (usando `strcmp`) y llama a la función correspondiente. En el caso específico del comando abort, primero verifica que no esté activo el shutdown antes de llamar a la función ejecutar_abort.
  5. Finalmente, libera la memoria del input (`free_user_input`) para evitar memory leaks antes de la siguiente vuelta del while.

---

### Supuestos o consideraciones

1. Asumimos que al lanzar un proceso sleep <time> este aunque la shell permita "pausarlo" y actualice ese estado en la tabla de procesos, este por detrás no es posible que sea pausado, por lo que en la shell se verá que el proceso finalizó incluso si se encontraba pausado. Esto último implica que el tiempo de ejecución que imprime la tabla solo considera el tiempo en el que al menos dentro la shell no estaba "pausado". 
2. Si bien no restringimos la cantidad de procesos vivos que podían haber al mismo tiempo, el arreglo de la tabla lo restringimos arbitrariamente a un máximo de 100 procesos que pueden ser almacenados en ella.
3. Hay algunas cosas que se imprimen en la shell que no son solicitadas en el enunciado, pero que usamos para debuggear y para entender bien como estaban ejecutándose nuestros procesos en la shell.