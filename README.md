# 💬 Reto 2 - Servidor y Cliente de Chat TCP Concurrente

# Este proyecto implementa un **servidor multicliente en C** que permite la conexión de múltiples clientes mediante **sockets TCP**. 
# Cada cliente puede unirse a una sala de chat, enviar mensajes, ver usuarios conectados y más. También incluye **persistencia de mensajes en archivos de texto**.

# 🛠️ Requisitos

# - ✅ Linux / WSL (se recomienda WSL2 con Ubuntu o Kali)
# - ✅ Compilador GCC (`build-essential`)
# - ✅ `make`
# - ✅ VS Code (opcional, recomendado)

# 📦 Estructura del proyecto

# Reto2/
# ├── Makefile
# ├── README.md
# ├── include/
# │   └── proto.h          # Header con estructuras y funciones compartidas
# ├── src/
# │   ├── server.c         # Código del servidor principal
# │   ├── rooms.c          # Lógica de manejo de salas
# │   ├── commands.c       # Comandos como /list, /users
# │   ├── persistence.c    # Persistencia de mensajes
# │   └── util.c           # Utilidades comunes
# ├── client/
# │   └── client.c         # Código del cliente
# ├── data/
# │   └── room_<sala>.log  # Archivos de persistencia de salas

# ⚙️ Compilación

# Ejecuta desde WSL o terminal de Linux:

# cd ~/Reto2
# sudo apt update
# sudo apt install -y build-essential libc6-dev linux-libc-dev
# make clean && make

# ▶️ Ejecución

# Abre 3 terminales diferentes:

# 🖥️ Terminal 1 - Ejecutar el servidor

# cd ~/Reto2
# ./server 9000

# 👤 Terminal 2 - Cliente 1

# cd ~/Reto2
# ./client_app 127.0.0.1 9000
# JOIN sala1 david
# MSG Hola a todos
# CMD /list
# CMD /users

# 👤 Terminal 3 - Cliente 2

# cd ~/Reto2
# ./client_app 127.0.0.1 9000
# JOIN sala1 ana
# MSG Hola David
# CMD /users

# 💬 Comandos disponibles

# Comando       | Descripción
# ------------- | -----------
# JOIN sala nick | Unirse a una sala con apodo
# MSG texto      | Enviar mensaje a todos
# CMD /list      | Listar salas activas
# CMD /users     | Ver usuarios en la sala
# CMD /leave     | Salir de la sala

# 📁 Persistencia

# Los mensajes se guardan automáticamente en la carpeta `data/` en archivos `room_<nombre_sala>.log`.

# Puedes ver el historial así:

# tail -n 20 data/room_sala1.log

# 👥 Autores del Proyecto

# Integrantes:

# -  Matias Martinez 
# -  Sofia Gallo 
# -  Juan Manuel Gallo 

# 📽️ Video de explicación

# El video debe incluir:

# - Introducción al proyecto
# - Explicación de la arquitectura
# - Demostración con 2 clientes conectados al servidor
# - Pruebas de comandos como /list, /users, /leave
# - Visualización de persistencia en archivos `.log`
# - Explicación por parte de los 3 integrantes (1 sección cada uno)

# ✅ Verificación final

# 1. Ejecutar `make clean && make`
# 2. Abrir 3 terminales (1 servidor, 2 clientes)
# 3. Conectarse con JOIN sala nick
# 4. Enviar mensajes con MSG
# 5. Probar comandos /list, /users, /leave
# 6. Confirmar que se guarda el archivo de la sala en data/

# 🧠 Comentarios finales

# Este sistema se desarrolló como ejercicio para aplicar:

# - Sockets TCP
# - Pthreads
# - Sincronización entre hilos
# - Comunicación entre múltiples clientes
# - Persistencia de mensajes
