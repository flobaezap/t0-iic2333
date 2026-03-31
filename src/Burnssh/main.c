#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../input_manager/manager.h"
#include <time.h>
#include <signal.h>
#include <sys/types.h> 

#define TRUE 1
#define MAX_PROCESSES 100

typedef struct {
    pid_t pid;
    char executable_name[256];
    time_t start_time;         
    int elapsed_time;         
    int is_paused;             
    int is_running;            
    int exit_code;             
    int signal_value;          
} Process;

Process process_table[MAX_PROCESSES];
int process_count = 0;
int shutdown_active = 0;       
time_t shutdown_start_time;
pid_t pending_aborts[MAX_PROCESSES];
int pending_aborts_count = 0;

void imprimir_proceso(Process p) {
    int current_time = p.elapsed_time;
    if (p.is_running) {
      current_time = (int)(time(NULL) - p.start_time);
      // hay que reformular esto cuando hagamos el pause
    }
    char* paused_str = p.is_paused ? "True" : "False";
    printf("%d %s %d %s %d %d\n", p.pid, p.executable_name, current_time, paused_str, p.exit_code, p.signal_value);
}

void actualizar_procesos(){
  int status;
  pid_t done_pid;
  while ((done_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (int i = 0; i < process_count; i++) {
      if (process_table[i].pid == done_pid && process_table[i].is_running) {
        process_table[i].is_running = 0;
        process_table[i].elapsed_time = (int)(time(NULL) - process_table[i].start_time);                
        if (WIFEXITED(status)) {
          process_table[i].exit_code = WEXITSTATUS(status);            
        } else if (WIFSIGNALED(status)) {
          process_table[i].signal_value = WTERMSIG(status);
        }     
        printf("\n[Background] El proceso '%s' (PID %d) ha finalizado.\n", process_table[i].executable_name, done_pid); // esto es para ver en consola como van los procesos
        break;
      }
    }
  }
}

void registrar_proceso(pid_t pid, char* executable) {
  if (process_count < MAX_PROCESSES) {
    process_table[process_count].pid = pid;
    strncpy(process_table[process_count].executable_name, executable, 255);
    process_table[process_count].executable_name[255] = '\0'; 
    process_table[process_count].start_time = time(NULL);
    process_table[process_count].elapsed_time = 0;
    process_table[process_count].is_paused = 0;
    process_table[process_count].is_running = 1;
    process_table[process_count].exit_code = -1;
    process_table[process_count].signal_value = -1;
    process_count++;
  } 
  else {
    printf("Error: No se pueden crear más procesos.\n");
  }
}

void ejecutar_launch(char** input){
  if (input[1] == NULL) {
    printf("Error: launch requiere el nombre de un ejecutable.\n");
    return;
  } 
  pid_t pid = fork();
  if (pid < 0) {
    perror("Error al crear el proceso");
  } 
  else if (pid == 0) {
    execvp(input[1], &input[1]);
    printf("Error: El ejecutable '%s' no existe o no se pudo ejecutar.\n", input[1]);
    exit(1);
  } 
  else {
    printf("Proceso '%s' lanzado en background con PID: %d\n", input[1], pid);
    registrar_proceso(pid, input[1]);
  }
}

void ejecutar_status() {
  if (process_count == 0) {
    printf("No hay procesos en el historial.\n");
    return;
  }
  for (int i = 0; i < process_count; i++) {
    imprimir_proceso(process_table[i]);
  }

}

void terminar_procesos(char** input) { //IH: en base a función execute_status y updated_finished_processes
  int time_to_wait = atoi(input[1]); // transforma el tiempo a esperar a int
  int pids_to_abort[MAX_PROCESSES];
  int corriendo = 0;

  for (int i = 0; i < process_count; i++) {
    if (process_table[i].is_running) {
      pids_to_abort[corriendo] = process_table[i].pid;
      corriendo++;
    }
  }

  if (corriendo == 0) {
    printf("No hay procesos  en ejecución. Abort no se puede ejecutar. \n");
    return;
  }

  pid_t abort_pid = fork();

  if (abort_pid == 0) {
    sleep(time_to_wait); 
    int abort_impreso = 0;

    for (int i = 0; i < corriendo; i++) {
      int pid_obj = pids_to_abort[i];
      if (kill(pid_obj, 0) == 0) {
        if (abort_impreso == 0) {
          printf("\nAbort cumplido.\n");
          abort_impreso = 1;
        }
        for (int j = 0; j < process_count; j++) {
          if (process_table[j].pid == pid_obj) {
            imprimir_proceso(process_table[j]);
            break;
          }
        }
        kill(pid_obj, SIGTERM);
      }
    }
    exit(0); 
  } else if (abort_pid > 0) {
    pending_aborts[pending_aborts_count] = abort_pid;
    pending_aborts_count++;
  }
}

void ejecutar_shutdown() {
  for (int i = 0; i < pending_aborts_count; i++) {
    kill(pending_aborts[i], SIGKILL);
  }
  pending_aborts_count = 0; 
  int corriendo = 0;

  for (int i = 0; i < process_count; i++) {
    if (process_table[i].is_running) {
      kill(process_table[i].pid, SIGINT);
      corriendo++;
    }
  }
  
  if (corriendo == 0) {
    printf("burnssh finalizado.\n");
    for (int i = 0; i < process_count; i++) {
      imprimir_proceso(process_table[i]);
    }
    exit(0);
  } else {
    shutdown_active = 1;
    shutdown_start_time = time(NULL);
  }
}

int main(int argc, char const *argv[])
{
  set_buffer(); // No borrar
  printf("Iniciando burnssh...\n");

  while (TRUE) {
    actualizar_procesos();

    if (shutdown_active) {
      int time_passed = (int)(time(NULL) - shutdown_start_time);
        
      if (time_passed >= 10) {
        for (int i = 0; i < process_count; i++) {
          if (process_table[i].is_running) {
            kill(process_table[i].pid, SIGKILL);
          }
        }
    
        usleep(50000); 
        actualizar_procesos(); 
        printf("burnssh finalizado.\n");
        for (int i = 0; i < process_count; i++) {
          imprimir_proceso(process_table[i]);
        }
        break; 
      }
    }

    printf("burnssh> ");
    fflush(stdout);
    char** input = read_user_input();

    if (input[0] == NULL || strcmp(input[0], "") == 0) {
      free_user_input(input);
      continue;
    }

    if (strcmp(input[0], "launch") == 0) {
      ejecutar_launch(input);
    }
    else if (strcmp(input[0], "status") == 0) {
      ejecutar_status();
    }
    else if (strcmp(input[0], "abort") == 0) { //IH: En base a update_finish_process
      if (shutdown_active) {
        printf("Comando abort anulado por shutdown en proceso.\n");
      } else {
        terminar_procesos(input);
      }
    }
    else if (strcmp(input[0], "shutdown") == 0) {
      if (!shutdown_active) {
        ejecutar_shutdown();
      }
    }
    else {
      printf("Comando no reconocido.\n");
    }

    free_user_input(input);
  }

  return 0;
}
