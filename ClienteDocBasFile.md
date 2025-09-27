# 🧾 Documentatación - Cliente de Chat en C 

Este programa implementa un **cliente de chat en C** que se comunica con un servidor utilizando **colas de mensajes (System V IPC)** y crea un **hilo de recepción de mensajes** mediante la biblioteca **pthreads**.

---

## 🔧 Funciones y Descripciones

| Función / Macro                        | Descripción según comentarios                                                                                      |
|----------------------------------------|--------------------------------------------------------------------------------------------------------------------|
| `strcpy(dest, src)`                    | Copia una cadena desde `src` a `dest`. Se usa para mover texto entre buffers o estructuras.                       |
| `strcmp(str1, str2)`                   | Compara dos cadenas carácter por carácter. Devuelve 0 si son iguales. Se usa para verificar el remitente.         |
| `strncmp(str1, str2, n)`              | Compara los primeros `n` caracteres de dos cadenas. Se usa para detectar comandos como `join`.                    |
| `fgets(str, size, stdin)`             | Lee una cadena desde la entrada estándar (teclado). Guarda hasta `size - 1` caracteres y añade `\0`.              |
| `strcspn(str1, str2)`                 | Devuelve la posición del primer carácter en `str1` que también esté en `str2`. Aquí se usa para eliminar `\n`.    |
| `ftok(path, id)`                      | Genera una clave única (`key_t`) a partir de una ruta y un identificador. Usado para crear claves de colas.       |
| `msgget(key, flags)`                  | Obtiene o crea una cola de mensajes. Requiere la clave generada por `ftok` y los permisos `flags`.                |
| `msgrcv(msqid, msgp, size, type, flags)` | Recibe un mensaje desde una cola. Se puede especificar el tipo de mensaje, tamaño y comportamiento.              |
| `msgsnd(msqid, msgp, size, flags)`    | Envía un mensaje a una cola de mensajes IPC.                                                                      |
| `perror(str)`                         | Imprime un mensaje de error si una función del sistema falla, junto con la causa del error.                       |
| `usleep(usec)`                        | Pausa la ejecución del hilo actual por una cantidad de microsegundos. Reduce el uso de CPU en ciclos constantes. |
| `pthread_t`                           | Tipo de dato que representa el identificador de un hilo.                                                          |
| `pthread_create(thread, attr, start_routine, arg)` | Crea un nuevo hilo. Recibe el identificador del hilo, atributos (o NULL), la función a ejecutar y un argumento.  |
| `atoi(str)`                           | Convierte una cadena a entero. Se usa para generar claves de cola con `ftok`, aunque es un método mejorable.     |
| `strlen(str)`                         | Devuelve la longitud de una cadena. Se usa para validar comandos o verificar si se ha unido a una sala.          |
| `sscanf(str, format, ...)`            | Extrae datos desde una cadena. Aquí se usa para leer el nombre de la sala desde el comando `join`.               |

---

## 🧵 Hilos

El programa crea un hilo con `pthread_create()` que ejecuta la función `recibir_mensajes()`, la cual se encarga de:

- Leer mensajes de la cola de la sala actual.
- Imprimir los mensajes que **no** sean del mismo usuario.
- Dormir brevemente con `usleep()` para evitar alto consumo de CPU.

---

## 💬 Comunicación entre procesos

- Usa **colas de mensajes (System V IPC)** para comunicación entre cliente y servidor.
- Hay una **cola global** para enviar solicitudes de tipo `JOIN` y recibir confirmaciones.
- Cada **sala** tiene su propia cola de mensajes para enviar y recibir mensajes entre usuarios.

---

## 🛠️ Notas adicionales

- El código asume que las salas son representables como enteros para usarlas con `ftok()`, lo cual **debería mejorarse**.
- No hay manejo de desconexión ni liberación de recursos (`msgctl`, etc.).
- No hay comando para salir de la sala o del programa (`Ctrl+C` es la única forma).

---

