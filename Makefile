# Makefile para el sistema de chat con colas de mensajes
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
LDFLAGS = -pthread

# Objetivos principales
all: servidor cliente

# Compilar el servidor
servidor: servidor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Compilar el cliente
cliente: cliente.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Limpiar archivos compilados
clean:
	rm -f servidor cliente *.o

# Limpiar colas de mensajes del sistema
clean-queues:
	ipcs -q | grep $(whoami) | awk '{print $$2}' | xargs -r ipcrm -q

# Limpiar todo (binarios y colas)
clean-all: clean clean-queues

# Instalar dependencias
install-deps:
	sudo apt update
	sudo apt install -y build-essential

.PHONY: all clean clean-queues clean-all install-deps
