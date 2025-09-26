/*
 * Reto 1: Sistema de Chat con Colas de Mensajes - CLIENTE
 *
 * VERSIÓN FINAL, COMPLETA Y COMENTADA
 *
 * Este cliente cumple con la arquitectura y el ejemplo del reto.
 *
 * FUNCIONALIDADES:
 * - Envía peticiones administrativas Y mensajes de chat a la cola global del servidor.
 * - El hilo receptor escucha en la cola de la sala y muestra TODOS los mensajes
 *   que el servidor reenvía, incluyendo los propios.
 */

 #define _POSIX_C_SOURCE 200809L
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/msg.h>
 #include <unistd.h>
 #include <pthread.h>
 #include <time.h>
 
 #define MAX_TEXTO 256
 #define MAX_NOMBRE 50
 
 struct mensaje {
     long mtype;
     char remitente[MAX_NOMBRE];
     char texto[MAX_TEXTO];
     char sala[MAX_NOMBRE];
 };
 
 int cola_global;
 int cola_sala = -1;
 char nombre_usuario[MAX_NOMBRE];
 char sala_actual[MAX_NOMBRE] = "";
 
 /**
  * Función ejecutada por un hilo para recibir mensajes de la sala de chat.
  * Se queda en un bucle infinito escuchando en la cola de la sala actual.
  * @param arg Argumentos del hilo (no se usa).
  * @return NULL.
  */
 void *recibir_mensajes(void *arg) {
     (void)arg;
     struct mensaje msg;
     while (1) {
         if (cola_sala != -1) {
             if (msgrcv(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) != -1) {
                 // Muestra el mensaje y redibuja el prompt para una mejor UX
                 printf("\r%s: %s\n> ", msg.remitente, msg.texto);
                 fflush(stdout);
             }
         }
         struct timespec ts = {0, 100000000L}; // Pausa de 100ms
         nanosleep(&ts, NULL);
     }
     return NULL;
 }
 
 /**
  * Función principal del cliente.
  * Se conecta a la cola global, inicia el hilo receptor y maneja la entrada del usuario.
  */
 int main(int argc, char *argv[]) {
     if (argc != 2) {
         printf("Uso: %s <nombre_usuario>\n", argv[0]);
         exit(1);
     }
     strcpy(nombre_usuario, argv[1]);
 
     key_t key_global = ftok("/tmp", 'A');
     cola_global = msgget(key_global, 0666);
     if (cola_global == -1) {
         perror("Error al conectar a la cola global");
         exit(1);
     }
 
     printf("Bienvenido, %s. Salas disponibles: General, Deportes\n", nombre_usuario);
 
     pthread_t hilo_receptor;
     pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL);
 
     struct mensaje msg;
     char comando[MAX_TEXTO];
 
     while (1) {
         printf("> ");
         if (fgets(comando, MAX_TEXTO, stdin) == NULL) {
             printf("\nSaliendo...\n");
             break;
         }
         comando[strcspn(comando, "\n")] = '\0';
 
         if (strncmp(comando, "join ", 5) == 0) {
             char sala[MAX_NOMBRE];
             sscanf(comando, "join %s", sala);
             msg.mtype = 1;
             strcpy(msg.remitente, nombre_usuario);
             strcpy(msg.sala, sala);
             msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
             msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0);
             if (atoi(msg.texto) > 0) {
                 cola_sala = atoi(msg.texto);
                 printf("Te has unido a la sala: %s\n", sala);
                 strcpy(sala_actual, sala);
             } else {
                 printf("Error al unirse: %s\n", strchr(msg.texto, '-') + 1);
             }
         } else if (strcmp(comando, "/list") == 0) {
             msg.mtype = 4;
             strcpy(msg.remitente, nombre_usuario);
             msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
             msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0);
             printf("%s\n", msg.texto);
         } else if (strcmp(comando, "/users") == 0) {
             if (strlen(sala_actual) == 0) { printf("No estás en ninguna sala.\n"); continue; }
             msg.mtype = 5;
             strcpy(msg.remitente, nombre_usuario);
             strcpy(msg.sala, sala_actual);
             msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
             msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0);
             printf("%s\n", msg.texto);
         } else if (strcmp(comando, "/leave") == 0) {
             if (strlen(sala_actual) == 0) { printf("No estás en ninguna sala.\n"); continue; }
             msg.mtype = 6;
             strcpy(msg.remitente, nombre_usuario);
             strcpy(msg.sala, sala_actual);
             msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
             msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0);
             printf("%s\n", msg.texto);
             strcpy(sala_actual, "");
             cola_sala = -1;
         } else if (strlen(comando) > 0) {
             if (strlen(sala_actual) == 0) {
                 printf("No estás en ninguna sala. Usa 'join <sala>' para unirte a una.\n");
                 continue;
             }
             msg.mtype = 3;
             strcpy(msg.remitente, nombre_usuario);
             strcpy(msg.sala, sala_actual);
             strcpy(msg.texto, comando);
             if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                 perror("Error al enviar mensaje");
             }
         }
     }
     return 0;
 }