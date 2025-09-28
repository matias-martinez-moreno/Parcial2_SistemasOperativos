# Sistema de Chat con Colas de Mensajes


## Estructura del Proyecto

```
Parcial2_SistemasOperativos/
├── servidor.c              # Servidor central con colas de mensajes
├── cliente.c               # Cliente que se conecta al servidor
├── Makefile                # Makefile para compilar el proyecto
├── README.md               # Documentación del proyecto
└── test_msgqueue.sh        # Script de prueba con instrucciones
```

## Compilación e Instalación

### Prerrequisitos

```bash
# Instalar herramientas de desarrollo (Ubuntu/Debian)
sudo apt update
sudo apt install -y build-essential

# Verificar que GCC esté instalado
gcc --version
```

### Compilación

```bash
# Compilar el proyecto completo
make

# Compilar solo el servidor
make servidor

# Compilar solo el cliente
make cliente

# Limpiar archivos compilados
make clean

# Limpiar colas de mensajes del sistema (si hay problemas)
make clean-queues
```

## Ejecución del Sistema

### 1. Ejecutar el Servidor

```bash
./servidor
```

**Salida esperada:**
```
Servidor de chat iniciado. Esperando clientes...
```

### 2. Ejecutar Clientes (en terminales separadas)

```bash
# Terminal 2 - Cliente María
./cliente María

# Terminal 3 - Cliente Juan  
./cliente Juan

# Terminal 4 - Cliente Camila
./cliente Camila
```

**Salida esperada del cliente:**
```
Bienvenido, María. Salas disponibles: General, Deportes
>
```

## Comandos Disponibles

| Comando | Descripción | Ejemplo |
|---------|-------------|---------|
| `join <sala>` | Unirse a una sala específica | `join General` |
| `<mensaje>` | Enviar mensaje a la sala actual | `Hola a todos!` |
| `/list` | Listar todas las salas disponibles | `/list` |
| `/users` | Listar usuarios en la sala actual | `/users` |
| `/leave` | Abandonar la sala actual | `/leave` |
| `Ctrl+C` | Salir del programa | - |


## Arquitectura del Sistema

### Componentes Principales

#### Servidor Central (`servidor.c`)
- Gestiona todo el sistema de chat
- Crea y gestiona colas de mensajes para cada sala
- Recibe mensajes de los clientes y los reenvía a todos los miembros de la sala
- Gestiona la lista de salas y usuarios en cada sala
- Utiliza una cola global para solicitudes de clientes

#### Cliente (`cliente.c`)
- Interfaz de usuario para el chat
- Se conecta al servidor mediante la cola global
- Se une a salas específicas
- Envía mensajes a la sala actual
- Recibe mensajes de otros usuarios mediante un hilo separado

### Flujo de Comunicación

1. Cliente envía mensaje JOIN a la cola global
2. Servidor procesa la solicitud y crea/encuentra la sala
3. Servidor responde con el ID de la cola de la sala
4. Cliente se conecta a la cola de la sala
5. Cliente envía mensajes a la cola global
6. Servidor reenvía mensajes a la cola de la sala
7. Todos los clientes de la sala reciben el mensaje

