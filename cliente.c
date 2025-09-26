/*
 * Cliente del Sistema de Chat con Colas de Mensajes (Message Queues)
 * 
 * Este archivo implementa el cliente que se conecta al servidor de chat
 * utilizando colas de mensajes System V como mecanismo de comunicación entre procesos (IPC).
 * 
 * Funcionalidades:
 * - Conectarse al servidor mediante cola global
 * - Unirse a salas de chat específicas
 * - Enviar mensajes a la sala actual
 * - Recibir mensajes de otros usuarios mediante hilo separado
 * - Interfaz de usuario interactiva
 * 
 * Autor: Equipo de Sistemas Operativos
 * Fecha: 2024
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

// Constantes del cliente
#define MAX_TEXTO 256                   // Longitud máxima del texto de mensaje
#define MAX_NOMBRE 50                   // Longitud máxima del nombre de usuario/sala

// Estructura para los mensajes que se envían entre procesos
struct mensaje {
    long mtype;                         // Tipo de mensaje (1=JOIN, 2=RESPUESTA, 3=MSG, 4=LIST_SALAS, 5=LIST_USERS, 6=LEAVE)
    char remitente[MAX_NOMBRE];         // Nombre del usuario que envía el mensaje
    char texto[MAX_TEXTO];              // Contenido del mensaje
    char sala[MAX_NOMBRE];              // Nombre de la sala destino
};

// Variables globales del cliente
int cola_global;                        // ID de la cola global del servidor
int cola_sala = -1;                     // ID de la cola de la sala actual (-1 = no conectado)
char nombre_usuario[MAX_NOMBRE];        // Nombre del usuario actual
char sala_actual[MAX_NOMBRE] = "";      // Nombre de la sala actual (vacío = no conectado)

/**
 * Función ejecutada en un hilo separado para recibir mensajes de la sala
 * 
 * Esta función se ejecuta continuamente en segundo plano para recibir
 * mensajes de otros usuarios en la sala actual.
 * 
 * @param arg: Parámetro del hilo (no usado)
 * @return: NULL
 */
void *recibir_mensajes(void *arg) {
    (void)arg; // Evitar warning de parámetro no usado
    struct mensaje msg;

    printf("Hilo receptor de mensajes iniciado\n");

    // Bucle infinito para recibir mensajes
    while (1) {
        // Solo intentar recibir si estamos conectados a una sala
        if (cola_sala != -1) {
            // Recibir mensajes de la cola de la sala
            if (msgrcv(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
                perror("Error al recibir mensaje de la sala");
                continue;
            }

            // Mostrar el mensaje solo si no es del propio usuario
            if (strcmp(msg.remitente, nombre_usuario) != 0) {
                printf("%s: %s\n", msg.remitente, msg.texto);
            }
        }
        
        // Pequeña pausa para no consumir demasiado CPU
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);
    }

    return NULL;
}

/**
 * Función principal del cliente
 * 
 * El cliente:
 * 1. Se conecta al servidor mediante la cola global
 * 2. Crea un hilo para recibir mensajes
 * 3. Proporciona una interfaz interactiva para el usuario
 * 4. Procesa comandos como "join" y mensajes de texto
 * 
 * @param argc: Número de argumentos
 * @param argv: Array de argumentos (debe incluir el nombre de usuario)
 * @return: 0 si éxito, 1 si error
 */
int main(int argc, char *argv[]) {
    // Verificar que se proporcione el nombre de usuario
    if (argc != 2) {
        printf("Uso incorrecto: %s <nombre_usuario>\n", argv[0]);
        printf("Ejemplo: %s María\n", argv[0]);
        exit(1);
    }

    // Guardar el nombre del usuario
    strcpy(nombre_usuario, argv[1]);
    printf("Bienvenido, %s!\n", nombre_usuario);

    // Conectarse a la cola global del servidor
    key_t key_global = ftok("/tmp", 'A');
    if (key_global == -1) {
        perror("Error al generar clave global");
        exit(1);
    }
    
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global del servidor");
        printf("Asegúrate de que el servidor esté ejecutándose\n");
        exit(1);
    }

    printf("Conectado al servidor exitosamente\n");
    printf("Salas disponibles: General, Deportes, Programación\n");
    printf("Comandos disponibles:\n");
    printf("   - join <sala>     : Unirse a una sala\n");
    printf("   - <mensaje>       : Enviar mensaje a la sala actual\n");
    printf("   - /list          : Listar todas las salas disponibles\n");
    printf("   - /users         : Listar usuarios en la sala actual\n");
    printf("   - /leave         : Abandonar la sala actual\n");
    printf("   - Ctrl+C         : Salir del programa\n\n");

    // Crear un hilo para recibir mensajes de forma asíncrona
    pthread_t hilo_receptor;
    if (pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL) != 0) {
        perror("Error al crear hilo receptor");
        exit(1);
    }

    struct mensaje msg;
    char comando[MAX_TEXTO];

    // Bucle principal de la interfaz de usuario
    while (1) {
        printf("> ");
        
        // Leer comando del usuario
        if (fgets(comando, MAX_TEXTO, stdin) == NULL) {
            printf("\n👋 Saliendo del programa...\n");
            break;
        }
        
        // Eliminar el salto de línea del final
        comando[strcspn(comando, "\n")] = '\0';

        // Procesar comandos especiales
        if (strcmp(comando, "/list") == 0) {
            // Solicitar lista de salas al servidor
            msg.mtype = 4; // Tipo LIST_SALAS
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, "");
            strcpy(msg.texto, "LIST_SALAS");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al solicitar lista de salas");
                continue;
            }
            
            printf("Solicitando lista de salas al servidor...\n");
            
        } else if (strcmp(comando, "/users") == 0) {
            // Solicitar lista de usuarios en la sala actual
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala\n");
                continue;
            }
            
            msg.mtype = 5; // Tipo LIST_USERS
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, "LIST_USERS");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al solicitar lista de usuarios");
                continue;
            }
            
            printf("Solicitando lista de usuarios en '%s'...\n", sala_actual);
            
        } else if (strcmp(comando, "/leave") == 0) {
            // Abandonar la sala actual
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala\n");
                continue;
            }
            
            msg.mtype = 6; // Tipo LEAVE
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, "LEAVE");
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al abandonar la sala");
                continue;
            }
            
            printf("Abandonando la sala '%s'...\n", sala_actual);
            strcpy(sala_actual, "");
            cola_sala = -1;
            
        } else if (strncmp(comando, "join ", 5) == 0) {
            char sala[MAX_NOMBRE];
            
            // Extraer el nombre de la sala del comando
            if (sscanf(comando, "join %s", sala) != 1) {
                printf("Uso: join <nombre_sala>\n");
                printf("Ejemplo: join General\n");
                continue;
            }

            printf("Intentando unirse a la sala '%s'...\n", sala);

            // Preparar mensaje de solicitud JOIN
            msg.mtype = 1; // Tipo JOIN
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala);
            strcpy(msg.texto, "");

            // Enviar solicitud al servidor
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de JOIN");
                continue;
            }

            // Esperar confirmación del servidor
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir confirmación del servidor");
                continue;
            }

            printf("✅ %s\n", msg.texto);

            // Conectarse a la cola de la sala usando el mismo algoritmo que el servidor
            // Buscar el índice de la sala para generar la clave correcta
            int indice_sala = 0;
            // Nota: En una implementación más robusta, el servidor enviaría el ID de la cola
            // Por ahora usamos una aproximación basada en el nombre de la sala
            if (strcmp(sala, "General") == 0) {
                indice_sala = 0;
            } else if (strcmp(sala, "Deportes") == 0) {
                indice_sala = 1;
            } else if (strcmp(sala, "Programacion") == 0) {
                indice_sala = 2;
            } else {
                // Para otras salas, usar hash del nombre
                indice_sala = strlen(sala) % 10;
            }
            
            key_t key_sala = ftok("/tmp", indice_sala + 1);
            cola_sala = msgget(key_sala, 0666);
            if (cola_sala == -1) {
                printf("Advertencia: No se pudo conectar a la cola de la sala '%s'\n", sala);
                printf("El sistema funcionará pero puede haber limitaciones\n");
            } else {
            printf("Conectado a la cola de la sala '%s' (ID: %d)\n", sala, cola_sala);
            }

            // Actualizar sala actual
            strcpy(sala_actual, sala);
            
        } else if (strlen(comando) > 0) {
            // Enviar un mensaje de texto a la sala actual
            
            // Verificar que el usuario esté en una sala
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala\n");
                printf("Usa 'join <sala>' para unirte a una sala primero\n");
                printf("Ejemplo: join General\n");
                continue;
            }

            // Preparar mensaje de texto
            msg.mtype = 3; // Tipo MSG
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, comando);

            // Enviar mensaje al servidor a través de la cola global
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje al servidor");
                continue;
            }

            printf("Mensaje enviado a la sala '%s'\n", sala_actual);
        }
    }

    printf("Hasta luego, %s!\n", nombre_usuario);
    return 0;
}