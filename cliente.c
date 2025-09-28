/*
 * Sistema de Chat con Colas de Mensajes - Cliente
 * 
 * Implementación del cliente que se conecta al servidor y permite
 * al usuario participar en salas de chat utilizando colas de mensajes.
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

// Constantes del sistema
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Estructura para los mensajes que circulan por las colas
struct mensaje {
    long mtype;                    // Tipo de mensaje
    char remitente[MAX_NOMBRE];    // Nombre del usuario que envía
    char texto[MAX_TEXTO];         // Contenido del mensaje
    char sala[MAX_NOMBRE];         // Nombre de la sala destino
};

// Variables globales del cliente
int cola_global;                    // ID de la cola global del servidor
int cola_sala = -1;                 // ID de la cola de la sala actual (-1 = no conectado)
char nombre_usuario[MAX_NOMBRE];    // Nombre del usuario actual
char sala_actual[MAX_NOMBRE] = "";  // Nombre de la sala actual

// Función ejecutada por un hilo separado para recibir mensajes
void *recibir_mensajes(void *arg) {
    (void)arg; // Evitar warning de parámetro no usado
    struct mensaje msg;
    
    // Bucle infinito para escuchar mensajes
    while (1) {
        if (cola_sala != -1) {
            // Intentar recibir un mensaje de la cola de la sala con timeout
            if (msgrcv(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0, IPC_NOWAIT) != -1) {
                // Solo mostrar mensajes de otros usuarios (no los propios)
                if (strcmp(msg.remitente, nombre_usuario) != 0) {
                    printf("\r%s: %s\n> ", msg.remitente, msg.texto);
                    fflush(stdout);
                }
            }
        }
        // Pequeña pausa para no consumir demasiado CPU
        struct timespec ts = {0, 100000000L}; // 100ms
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Verificar argumentos de línea de comandos
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }
    
    // Validar longitud del nombre de usuario
    if (strlen(argv[1]) >= MAX_NOMBRE) {
        printf("Error: El nombre de usuario es demasiado largo (máximo %d caracteres)\n", MAX_NOMBRE - 1);
        exit(1);
    }
    
    // Validar que el nombre no esté vacío
    if (strlen(argv[1]) == 0) {
        printf("Error: El nombre de usuario no puede estar vacío\n");
        exit(1);
    }
    
    strcpy(nombre_usuario, argv[1]);

    // Conectarse a la cola global del servidor
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    printf("Bienvenido, %s. Salas disponibles: General, Deportes\n", nombre_usuario);

    // Crear un hilo separado para recibir mensajes
    pthread_t hilo_receptor;
    pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL);

    struct mensaje msg;
    char comando[MAX_TEXTO];

    // Bucle principal - procesar comandos del usuario
    while (1) {
        printf("> ");
        if (fgets(comando, MAX_TEXTO, stdin) == NULL) {
            printf("\nSaliendo...\n");
            break;
        }
        comando[strcspn(comando, "\n")] = '\0'; // Eliminar salto de línea

        // Procesar comando "join <sala>"
        if (strncmp(comando, "join ", 5) == 0) {
            char sala[MAX_NOMBRE];
            sscanf(comando, "join %s", sala);
            
            // Preparar mensaje de JOIN
            msg.mtype = 1;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala);
            strcpy(msg.texto, "");
            
            // Enviar solicitud al servidor
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de JOIN");
                continue;
            }
            
            // Esperar respuesta del servidor
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir confirmación");
                continue;
            }
            
            // Procesar respuesta
            int cola_id = atoi(msg.texto);
            if (cola_id > 0) {
                cola_sala = cola_id;
                printf("Te has unido a la sala: %s\n", sala);
                strcpy(sala_actual, sala);
                
                // Limpiar mensajes residuales de la cola de la sala
                struct mensaje msg_residual;
                while (msgrcv(cola_sala, &msg_residual, sizeof(struct mensaje) - sizeof(long), 0, IPC_NOWAIT) != -1) {
                    // Descartar mensajes residuales
                }
            } else {
                printf("Error al unirse: %s\n", msg.texto);
            }
        }
        // Comando "/list" - listar salas disponibles
        else if (strcmp(comando, "/list") == 0) {
            msg.mtype = 4;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, "");
            strcpy(msg.texto, "");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de lista");
                continue;
            }
            
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir lista de salas");
                continue;
            }
            
            printf("%s\n", msg.texto);
        }
        // Comando "/users" - listar usuarios de la sala actual
        else if (strcmp(comando, "/users") == 0) {
            if (strlen(sala_actual) == 0) { 
                printf("No estás en ninguna sala.\n"); 
                continue; 
            }
            msg.mtype = 5;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, "");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de usuarios");
                continue;
            }
            
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir lista de usuarios");
                continue;
            }
            
            printf("%s\n", msg.texto);
        }
        // Comando "/leave" - salir de la sala actual
        else if (strcmp(comando, "/leave") == 0) {
            if (strlen(sala_actual) == 0) { 
                printf("No estás en ninguna sala.\n"); 
                continue; 
            }
            msg.mtype = 6;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, "");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de salida");
                continue;
            }
            
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir confirmación de salida");
                continue;
            }
            
            printf("%s\n", msg.texto);
            strcpy(sala_actual, "");
            cola_sala = -1;
            
            // Limpiar mensajes residuales de la cola anterior
            struct mensaje msg_residual;
            while (msgrcv(cola_global, &msg_residual, sizeof(struct mensaje) - sizeof(long), 0, IPC_NOWAIT) != -1) {
                // Descartar mensajes residuales
            }
        }
        // Cualquier otro texto se considera un mensaje de chat
        else if (strlen(comando) > 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>' para unirte a una.\n");
                continue;
            }
            
            // Enviar mensaje de chat
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