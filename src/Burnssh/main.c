#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../input_manager/manager.h"
#include <time.h>

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

void update_finished_processes(){
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

void register_new_process(pid_t pid, char* executable) {
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

void execute_launch(char** input){
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
    register_new_process(pid, input[1]);
  }
}

void execute_status(char** input) {
  if (process_count == 0) {
        printf("No hay procesos en el historial.\n");
        return;
  }

  for (int i = 0; i < process_count; i++) {
    Process p = process_table[i];
    int current_time = p.elapsed_time;
    if (p.is_running) {
      current_time = (int)(time(NULL) - p.start_time);
      // hay que reformular esto cuando hagamos el pause para que no cuente el tiempo de pausa
    }
    char* paused_str = p.is_paused ? "True" : "False";
    printf("%d %s %d %s %d %d\n", 
      p.pid, 
      p.executable_name, 
      current_time, 
      paused_str, 
      p.exit_code, 
      p.signal_value);
  }
  printf("\n");
}


int main(int argc, char const *argv[])
{
  set_buffer(); // No borrar
  printf("Iniciando burnssh...\n");

  while (TRUE) {
    update_finished_processes();

    printf("burnssh> ");
    fflush(stdout);
    char** input = read_user_input();

    if (input[0] == NULL || strcmp(input[0], "") == 0) {
      free_user_input(input);
      continue;
    }

    if (strcmp(input[0], "launch") == 0) {
      execute_launch(input);
    }
    else if (strcmp(input[0], "status") == 0) {
      execute_status(input);
    }
    else if (strcmp(input[0], "exit") == 0) {
      free_user_input(input);
      break;
    }
    else {
      printf("Comando no reconocido.\n");
    }

    free_user_input(input);
  }

  return 0;
}
