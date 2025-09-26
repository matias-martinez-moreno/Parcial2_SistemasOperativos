/*
 * Sistema de Chat con Colas de Mensajes (Message Queues)
 * 
 * Este archivo implementa el servidor central que gestiona las salas de chat
 * utilizando colas de mensajes System V como mecanismo de comunicación entre procesos (IPC).
 * 
 * Funcionalidades:
 * - Crear y gestionar múltiples salas de chat
 * - Cada sala tiene su propia cola de mensajes
 * - Permitir que usuarios se unan a salas
 * - Reenviar mensajes a todos los usuarios de una sala
 * - Gestionar usuarios por sala
 * 
 * Autor: Equipo de Sistemas Operativos
 * Fecha: 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

// Constantes del sistema
#define MAX_SALAS 10                    // Máximo número de salas permitidas
#define MAX_USUARIOS_POR_SALA 20        // Máximo usuarios por sala
#define MAX_TEXTO 256                   // Longitud máxima del texto de mensaje
#define MAX_NOMBRE 50                   // Longitud máxima del nombre de usuario/sala

// Estructura para los mensajes que se envían entre procesos
struct mensaje {
    long mtype;                         // Tipo de mensaje (1=JOIN, 2=RESPUESTA, 3=MSG, 4=LIST_SALAS, 5=LIST_USERS, 6=LEAVE)
    char remitente[MAX_NOMBRE];         // Nombre del usuario que envía el mensaje
    char texto[MAX_TEXTO];              // Contenido del mensaje
    char sala[MAX_NOMBRE];              // Nombre de la sala destino
};

// Estructura que representa una sala de chat
struct sala {
    char nombre[MAX_NOMBRE];            // Nombre de la sala
    int cola_id;                        // ID de la cola de mensajes de la sala
    int num_usuarios;                   // Número actual de usuarios en la sala
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE]; // Lista de usuarios en la sala
};

// Variables globales del servidor
struct sala salas[MAX_SALAS];           // Array de todas las salas del sistema
int num_salas = 0;                      // Contador de salas creadas

/**
 * Función para crear una nueva sala de chat
 * 
 * @param nombre: Nombre de la sala a crear
 * @return: Índice de la sala creada, o -1 si hay error
 */
int crear_sala(const char *nombre) {
    // Verificar si se puede crear más salas
    if (num_salas >= MAX_SALAS) {
        printf("Error: No se pueden crear más salas (límite: %d)\n", MAX_SALAS);
        return -1;
    }

    // Generar una clave única para la cola de mensajes de la sala
    key_t key = ftok("/tmp", num_salas + 1);
    if (key == -1) {
        perror("Error al generar clave para la sala");
        return -1;
    }

    // Crear la cola de mensajes para la sala
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    // Inicializar la estructura de la sala
    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;

    printf("Sala '%s' creada exitosamente (ID cola: %d)\n", nombre, cola_id);
    num_salas++;
    return num_salas - 1; // Retornar el índice de la sala creada
}

/**
 * Función para buscar una sala por su nombre
 * 
 * @param nombre: Nombre de la sala a buscar
 * @return: Índice de la sala encontrada, o -1 si no existe
 */
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1; // Sala no encontrada
}

/**
 * Función para agregar un usuario a una sala específica
 * 
 * @param indice_sala: Índice de la sala en el array
 * @param nombre_usuario: Nombre del usuario a agregar
 * @return: 0 si éxito, -1 si hay error
 */
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
    // Verificar que el índice de sala sea válido
    if (indice_sala < 0 || indice_sala >= num_salas) {
        printf("Error: Índice de sala inválido\n");
        return -1;
    }

    struct sala *s = &salas[indice_sala];
    
    // Verificar si la sala está llena
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) {
        printf("Error: La sala '%s' está llena (máximo: %d usuarios)\n", 
               s->nombre, MAX_USUARIOS_POR_SALA);
        return -1;
    }

    // Verificar si el usuario ya está en la sala
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
            printf("Error: El usuario '%s' ya está en la sala '%s'\n", 
                   nombre_usuario, s->nombre);
            return -1;
        }
    }

    // Agregar el usuario a la sala
    strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
    s->num_usuarios++;
    
    printf("Usuario '%s' agregado a la sala '%s' (usuarios: %d/%d)\n", 
           nombre_usuario, s->nombre, s->num_usuarios, MAX_USUARIOS_POR_SALA);
    return 0;
}

/**
 * Función para enviar un mensaje a todos los usuarios de una sala
 * 
 * @param indice_sala: Índice de la sala destino
 * @param msg: Puntero al mensaje a enviar
 */
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg) {
    // Verificar que el índice de sala sea válido
    if (indice_sala < 0 || indice_sala >= num_salas) {
        printf("Error: Índice de sala inválido para envío de mensaje\n");
        return;
    }

    struct sala *s = &salas[indice_sala];
    
    // Enviar el mensaje a todos los usuarios de la sala
    for (int i = 0; i < s->num_usuarios; i++) {
        if (msgsnd(s->cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("Error al enviar mensaje a la sala");
        }
    }
    
    printf("Mensaje enviado a %d usuarios en la sala '%s'\n", 
           s->num_usuarios, s->nombre);
}

/**
 * Función principal del servidor
 * 
 * El servidor:
 * 1. Crea una cola global para recibir solicitudes de clientes
 * 2. Escucha mensajes de tipo JOIN y MSG
 * 3. Gestiona las salas y usuarios
 * 4. Reenvía mensajes a todos los usuarios de cada sala
 */
int main() {
    printf("Iniciando servidor de chat con colas de mensajes...\n");
    
    // Crear la cola global para solicitudes de clientes
    key_t key_global = ftok("/tmp", 'A');
    if (key_global == -1) {
        perror("Error al generar clave global");
        exit(1);
    }
    
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado exitosamente\n");
    printf("Cola global creada (ID: %d)\n", cola_global);
    printf("Esperando clientes...\n\n");

    struct mensaje msg;

    // Bucle principal del servidor
    while (1) {
        // Recibir mensajes de la cola global
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        // Procesar el mensaje según su tipo
        if (msg.mtype == 1) { // Mensaje tipo JOIN (unirse a sala)
            printf("Solicitud de unirse a la sala '%s' por '%s'\n", 
                   msg.sala, msg.remitente);

            // Buscar si la sala ya existe
            int indice_sala = buscar_sala(msg.sala);
            
            // Si la sala no existe, crearla
            if (indice_sala == -1) {
                indice_sala = crear_sala(msg.sala);
                if (indice_sala == -1) {
                    printf("No se pudo crear la sala '%s'\n", msg.sala);
                    continue;
                }
            }

            // Agregar el usuario a la sala
            if (agregar_usuario_a_sala(indice_sala, msg.remitente) == 0) {
                // Enviar confirmación al cliente
                msg.mtype = 2; // Tipo de respuesta
                sprintf(msg.texto, "Te has unido a la sala: %s", msg.sala);
                
                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar confirmación");
                } else {
                    printf("Confirmación enviada a '%s'\n", msg.remitente);
                }
            } else {
                printf("No se pudo agregar al usuario '%s' a la sala '%s'\n", 
                       msg.remitente, msg.sala);
            }
            
        } else if (msg.mtype == 3) { // Mensaje tipo MSG (mensaje de chat)
            printf("Mensaje en la sala '%s' de '%s': %s\n", 
                   msg.sala, msg.remitente, msg.texto);

            // Buscar la sala destino
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                // Reenviar el mensaje a todos en la sala
                enviar_a_todos_en_sala(indice_sala, &msg);
            } else {
                printf("Sala '%s' no encontrada para el mensaje\n", msg.sala);
            }
            
        } else if (msg.mtype == 4) { // LIST_SALAS
            printf("Solicitud de lista de salas por '%s'\n", msg.remitente);
            
            // Enviar lista de salas al cliente
            msg.mtype = 2; // Tipo de respuesta
            strcpy(msg.texto, "SALAS_DISPONIBLES:");
            
            for (int i = 0; i < num_salas; i++) {
                char temp[MAX_TEXTO];
                sprintf(temp, " %s(%d usuarios)", salas[i].nombre, salas[i].num_usuarios);
                strcat(msg.texto, temp);
            }
            
            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar lista de salas");
            }
            
        } else if (msg.mtype == 5) { // LIST_USERS
            printf("Solicitud de usuarios en '%s' por '%s'\n", msg.sala, msg.remitente);
            
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                struct sala *s = &salas[indice_sala];
                
                msg.mtype = 2; // Tipo de respuesta
                sprintf(msg.texto, "USUARIOS_EN_%s:", msg.sala);
                
                for (int i = 0; i < s->num_usuarios; i++) {
                    char temp[MAX_TEXTO];
                    sprintf(temp, " %s", s->usuarios[i]);
                    strcat(msg.texto, temp);
                }
                
                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar lista de usuarios");
                }
            }
            
        } else if (msg.mtype == 6) { // LEAVE
            printf("Solicitud de abandonar '%s' por '%s'\n", msg.sala, msg.remitente);
            
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                struct sala *s = &salas[indice_sala];
                
                // Buscar y eliminar el usuario de la sala
                for (int i = 0; i < s->num_usuarios; i++) {
                    if (strcmp(s->usuarios[i], msg.remitente) == 0) {
                        // Mover usuarios restantes hacia atrás
                        for (int j = i; j < s->num_usuarios - 1; j++) {
                            strcpy(s->usuarios[j], s->usuarios[j + 1]);
                        }
                        s->num_usuarios--;
                        printf("Usuario '%s' removido de la sala '%s'\n", msg.remitente, msg.sala);
                        break;
                    }
                }
                
                // Enviar confirmación
                msg.mtype = 2;
                sprintf(msg.texto, "Has abandonado la sala: %s", msg.sala);
                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar confirmación de salida");
                }
            }
        }
        
        printf("---\n"); // Separador visual
    }

    return 0;
}