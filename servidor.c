/*
 * Sistema de Chat con Colas de Mensajes - Servidor
 * 
 * Implementación del servidor central que gestiona las salas de chat
 * utilizando colas de mensajes System V para comunicación entre procesos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>

// Constantes del sistema
#define MAX_SALAS 10
#define MAX_USUARIOS_POR_SALA 20
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Estructura para los mensajes que circulan por las colas
struct mensaje {
    long mtype;                    // Tipo de mensaje (1=JOIN, 2=RESP, 3=MSG, etc.)
    char remitente[MAX_NOMBRE];    // Nombre del usuario que envía
    char texto[MAX_TEXTO];         // Contenido del mensaje
    char sala[MAX_NOMBRE];         // Nombre de la sala destino
};

// Estructura que representa una sala de chat
struct sala {
    char nombre[MAX_NOMBRE];                           // Nombre de la sala
    int cola_id;                                      // ID de la cola de mensajes
    int num_usuarios;                                 // Cantidad de usuarios conectados
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE]; // Lista de usuarios
};

// Variables globales del servidor
struct sala salas[MAX_SALAS];  // Array de salas disponibles
int num_salas = 0;             // Contador de salas creadas

// Declaraciones de funciones
int crear_sala(const char *nombre);
int buscar_sala(const char *nombre);
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario);
int remover_usuario_de_sala(int indice_sala, const char *nombre_usuario);
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg);

int main() {
    // Crear la cola global para recibir solicitudes de los clientes
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }
    
    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;

    // Bucle principal del servidor - procesa mensajes indefinidamente
    while (1) {
        // Esperar cualquier tipo de mensaje en la cola global
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        // Procesar el mensaje según su tipo
        switch (msg.mtype) {
            case 1: { // JOIN - Cliente quiere unirse a una sala
                printf("Solicitud de JOIN: usuario '%s' quiere unirse a sala '%s'\n", msg.remitente, msg.sala);
                msg.mtype = 2; // Preparar respuesta
                
                // Buscar si la sala ya existe
                int indice_sala = buscar_sala(msg.sala);
                if (indice_sala == -1) {
                    // Sala no existe, crearla
                    printf("Sala '%s' no existe, creándola...\n", msg.sala);
                    indice_sala = crear_sala(msg.sala);
                    if (indice_sala == -1) {
                        printf("Error: No se pudo crear la sala '%s'\n", msg.sala);
                        sprintf(msg.texto, "0-Error: No se pudo crear la sala.");
                        msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                        break;
                    }
                    printf("Sala '%s' creada exitosamente (ID cola: %d)\n", msg.sala, salas[indice_sala].cola_id);
                } else {
                    printf("Sala '%s' encontrada (ID cola: %d)\n", msg.sala, salas[indice_sala].cola_id);
                }
                
                // Intentar agregar el usuario a la sala
                int resultado = agregar_usuario_a_sala(indice_sala, msg.remitente);
                if (resultado == 0) {
                    printf("Usuario '%s' agregado exitosamente a sala '%s' (usuarios: %d/%d)\n", 
                           msg.remitente, msg.sala, salas[indice_sala].num_usuarios, MAX_USUARIOS_POR_SALA);
                    sprintf(msg.texto, "%d", salas[indice_sala].cola_id);
                } else {
                    printf("Error: No se pudo agregar usuario '%s' a sala '%s' (código error: %d)\n", 
                           msg.remitente, msg.sala, resultado);
                    sprintf(msg.texto, "0-Error: El nombre de usuario ya existe o la sala está llena.");
                }
                msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                break;
            }
            case 3: { // MSG - Mensaje de chat que hay que reenviar
                int indice_sala = buscar_sala(msg.sala);
                if (indice_sala != -1) {
                    enviar_a_todos_en_sala(indice_sala, &msg);
                }
                break;
            }
            case 4: { // LIST_SALAS - Cliente pide lista de salas
                msg.mtype = 2;
                if (num_salas == 0) {
                    strcpy(msg.texto, "No hay salas activas.");
                } else {
                    strcpy(msg.texto, "Salas disponibles:");
                    for (int i = 0; i < num_salas; i++) {
                        char temp[MAX_TEXTO];
                        sprintf(temp, "\n - %s (%d/%d)", salas[i].nombre, salas[i].num_usuarios, MAX_USUARIOS_POR_SALA);
                        strcat(msg.texto, temp);
                    }
                }
                msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                break;
            }
            case 5: { // LIST_USERS - Cliente pide lista de usuarios de su sala
                msg.mtype = 2;
                int indice_sala = buscar_sala(msg.sala);
                if (indice_sala != -1) {
                    struct sala *s = &salas[indice_sala];
                    sprintf(msg.texto, "Usuarios en '%s':", msg.sala);
                    for (int i = 0; i < s->num_usuarios; i++) {
                        strcat(msg.texto, "\n - ");
                        strcat(msg.texto, s->usuarios[i]);
                    }
                } else {
                    strcpy(msg.texto, "Error: Sala no encontrada.");
                }
                msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                break;
            }
            case 6: { // LEAVE - Cliente quiere salir de la sala
                msg.mtype = 2;
                int indice_sala = buscar_sala(msg.sala);
                if (indice_sala != -1) {
                    remover_usuario_de_sala(indice_sala, msg.remitente);
                }
                sprintf(msg.texto, "Has abandonado la sala '%s'.", msg.sala);
                msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                break;
            }
        }
    }
    return 0;
}

// Función que reenvía un mensaje a todos los usuarios de una sala
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg) {
    struct sala *s = &salas[indice_sala];
    // Enviar el mensaje múltiples veces (una por cada usuario en la sala)
    for (int i = 0; i < s->num_usuarios; i++) {
        if (msgsnd(s->cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("Error al reenviar mensaje a la sala");
        }
    }
}

// Crea una nueva sala de chat con su propia cola de mensajes
int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) return -1;
    
    // Generar una clave única para la cola de esta sala usando timestamp
    key_t key = ftok("/tmp", 'B' + num_salas + time(NULL));
    int cola_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (cola_id == -1) {
        // Si falla, intentar con una clave diferente
        key = ftok("/tmp", 'C' + num_salas + getpid());
        cola_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
        if (cola_id == -1) return -1;
    }
    
    // Inicializar la estructura de la sala
    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;
    num_salas++;
    
    return num_salas - 1; // Retornar el índice de la sala creada
}

// Busca una sala por su nombre en el array de salas
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) return i;
    }
    return -1; // Sala no encontrada
}

// Agrega un usuario a una sala específica
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
    struct sala *s = &salas[indice_sala];
    
    // Verificar que la sala no esté llena
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) return -1;
    
    // Verificar que el usuario no esté ya en la sala
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) return -1;
    }
    
    // Agregar el usuario a la lista
    strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
    s->num_usuarios++;
    
    return 0; // Éxito
}

// Remueve un usuario de una sala específica
int remover_usuario_de_sala(int indice_sala, const char *nombre_usuario) {
    struct sala *s = &salas[indice_sala];
    int encontrado = -1;
    
    // Buscar el usuario en la lista
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
            encontrado = i;
            break;
        }
    }
    
    // Si se encontró, removerlo y compactar la lista
    if (encontrado != -1) {
        for (int i = encontrado; i < s->num_usuarios - 1; i++) {
            strcpy(s->usuarios[i], s->usuarios[i + 1]);
        }
        s->num_usuarios--;
    }
    
    return 0;
}