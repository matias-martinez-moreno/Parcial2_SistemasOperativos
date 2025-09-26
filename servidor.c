/*
 * Reto 1: Sistema de Chat con Colas de Mensajes - SERVIDOR
 *
 * VERSIÓN FINAL Y CORRECTA - Cumple con la arquitectura del reto.
 *
 * FUNCIONALIDADES:
 * - Crea una cola global para peticiones administrativas (JOIN, LIST, etc.).
 * - Al recibir un JOIN, crea una sala con su propia cola y devuelve el ID de esa cola.
 * - Gestiona la lista de usuarios y salas.
 * - Responde a los comandos /list, /users, /leave.
 * - NO interviene en el envío de mensajes de chat entre usuarios.
 */

 #include <stdio.hh>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/msg.h>
 #include <unistd.h>
 
 #define MAX_SALAS 10
 #define MAX_USUARIOS_POR_SALA 20
 #define MAX_TEXTO 256
 #define MAX_NOMBRE 50
 
 // Estructura para los mensajes (unificada para peticiones y chat)
 struct mensaje {
     long mtype;
     char remitente[MAX_NOMBRE];
     char texto[MAX_TEXTO];
     char sala[MAX_NOMBRE];
 };
 
 // Estructura para una sala de chat
 struct sala {
     char nombre[MAX_NOMBRE];
     int cola_id;
     int num_usuarios;
     char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE];
 };
 
 struct sala salas[MAX_SALAS];
 int num_salas = 0;
 
 // Prototipos de funciones auxiliares
 int crear_sala(const char *nombre);
 int buscar_sala(const char *nombre);
 int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario);
 int remover_usuario_de_sala(int indice_sala, const char *nombre_usuario);
 
 int main() {
     printf("Iniciando servidor de chat...\n");
     key_t key_global = ftok("/tmp", 'A');
     int cola_global = msgget(key_global, IPC_CREAT | 0666);
     if (cola_global == -1) {
         perror("Error al crear la cola global");
         exit(1);
     }
     printf("✅ Servidor iniciado. Cola global ID: %d. Esperando peticiones...\n\n", cola_global);
 
     struct mensaje msg;
 
     // Bucle principal del servidor: solo escucha en la cola global
     while (1) {
         if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
             perror("Error al recibir mensaje de la cola global");
             continue;
         }
 
         // Todas las respuestas del servidor al cliente usan mtype = 2
         long tipo_peticion = msg.mtype;
         msg.mtype = 2;
 
         switch (tipo_peticion) {
             case 1: { // JOIN
                 printf("[PETICION] JOIN a '%s' por '%s'\n", msg.sala, msg.remitente);
                 int indice_sala = buscar_sala(msg.sala);
                 if (indice_sala == -1) {
                     indice_sala = crear_sala(msg.sala);
                 }
 
                 if (indice_sala != -1 && agregar_usuario_a_sala(indice_sala, msg.remitente) == 0) {
                     // Éxito: Devolver el ID de la cola de la sala
                     sprintf(msg.texto, "%d", salas[indice_sala].cola_id);
                 } else {
                     // Error: Devolver un ID inválido y un mensaje de error
                     sprintf(msg.texto, "0-Error: El nombre de usuario ya existe o la sala está llena.");
                 }
                 msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                 break;
             }
             case 4: { // LIST_SALAS
                 printf("[PETICION] LIST_SALAS por '%s'\n", msg.remitente);
                 if (num_salas == 0) {
                     strcpy(msg.texto, "No hay salas activas en este momento.");
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
             case 5: { // LIST_USERS
                 printf("[PETICION] LIST_USERS en '%s' por '%s'\n", msg.sala, msg.remitente);
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
             case 6: { // LEAVE
                 printf("[PETICION] LEAVE de '%s' por '%s'\n", msg.sala, msg.remitente);
                 int indice_sala = buscar_sala(msg.sala);
                 if (indice_sala != -1) {
                     remover_usuario_de_sala(indice_sala, msg.remitente);
                 }
                 sprintf(msg.texto, "Has abandonado la sala '%s'.", msg.sala);
                 msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
                 break;
             }
         }
         printf("---\n");
     }
 
     return 0;
 }
 
 // Implementación de funciones auxiliares
 
 int crear_sala(const char *nombre) {
     if (num_salas >= MAX_SALAS) return -1;
     key_t key = ftok("/tmp", num_salas + 1);
     int cola_id = msgget(key, IPC_CREAT | 0666);
     if (cola_id == -1) return -1;
     strcpy(salas[num_salas].nombre, nombre);
     salas[num_salas].cola_id = cola_id;
     salas[num_salas].num_usuarios = 0;
     printf("INFO: Sala '%s' creada (ID cola: %d)\n", nombre, cola_id);
     num_salas++;
     return num_salas - 1;
 }
 
 int buscar_sala(const char *nombre) {
     for (int i = 0; i < num_salas; i++) {
         if (strcmp(salas[i].nombre, nombre) == 0) return i;
     }
     return -1;
 }
 
 int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
     struct sala *s = &salas[indice_sala];
     if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) {
         printf("ERROR: La sala '%s' está llena.\n", s->nombre);
         return -1;
     }
     for (int i = 0; i < s->num_usuarios; i++) {
         if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
             printf("ERROR: Usuario '%s' ya existe en la sala '%s'.\n", nombre_usuario, s->nombre);
             return -1;
         }
     }
     strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
     s->num_usuarios++;
     printf("INFO: Usuario '%s' agregado a '%s'\n", nombre_usuario, s->nombre);
     return 0;
 }
 
 int remover_usuario_de_sala(int indice_sala, const char *nombre_usuario) {
     struct sala *s = &salas[indice_sala];
     int encontrado = -1;
     for (int i = 0; i < s->num_usuarios; i++) {
         if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
             encontrado = i;
             break;
         }
     }
     if (encontrado != -1) {
         for (int i = encontrado; i < s->num_usuarios - 1; i++) {
             strcpy(s->usuarios[i], s->usuarios[i + 1]);
         }
         s->num_usuarios--;
         printf("INFO: Usuario '%s' removido de '%s'\n", nombre_usuario, s->nombre);
     }
     return 0;
 }