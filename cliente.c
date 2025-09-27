#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Estructura para los mensajes
struct mensaje {
    long mtype;         // Tipo de mensaje
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

int cola_global;
int cola_sala = -1;
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";

// Función para el hilo que recibe mensajes
void *recibir_mensajes(void *arg) {
    struct mensaje msg;

    while (1) {
        if (cola_sala != -1) {
            // Recibir mensajes de la cola de la sala
            if (msgrcv(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) { // Esta función lee un mensaje en una cola de mensajes
                perror("Error al recibir mensaje de la sala");
                continue;
            }

            // Mostrar el mensaje si no es del propio usuario
            if (strcmp(msg.remitente, nombre_usuario) != 0) { // Permite comparar caracter por caracter de dos cadenas, mirar si son iguales menor o mayor . En este caso se mira si son diferentes
                printf("%s: %s\n", msg.remitente, msg.texto);
            }
        }
        usleep(100000); // Pequeña pausa para no consumir demasiado CPU
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }

    strcpy(nombre_usuario, argv[1]);  // Esta función tiene 2 argumentos ( 1. dirección destino, 2. dirección origen) sirve para mover una cadena, copiarla de una dirección a otra de la memoria. 

    // Conectarse a la cola global
    key_t key_global = ftok("/tmp", 'A'); // La función ftok genera una clave unica de tipo ftok y tiene los siguientes parametros : 1. Ruta de un archivo existente en el sistema, 2. Un entero que funciona como el identificador del proyecto. Por lo que yo veo esto funciona como un clave valor. 


    cola_global = msgget(key_global, 0666);  // Sirve para traer la clave de un mensaje en una cola y tiene los siguientes parametros ( 1. identificador de la cola, en este caso de key_global (Debe ser de tipo key_t), 2. son flags para el comportamiento de la acción)
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    printf("Bienvenido, %s. Salas disponibles: General, Deportes\n", nombre_usuario);

    // Crear un hilo para recibir mensajes
    pthread_t hilo_receptor; // Aquí se está creando el identificador del hilo (por cierto , esto es un puntero)
    pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL); // Como su nombre lo indica, esta función permite crear un hilo y tiene los siguiente parametros: 1. el identificador del hilo que se va a crear, 2. Un puntero a la estructura con las propiedades que se quiere que se tengan en el hilo (NULL quiere decir que se usarán los valores predeterminados) , 3. La dirección de la función que el nuevo hilo ejecutará , 4. Un argumento que se le pasará a la start_routine, esto puede referenciar a cualquier tipo de datos ( es basicamente el argumento que se le creará a la función de inicio de routina)


    struct mensaje msg;
    char comando[MAX_TEXTO];

    while (1) {
        printf("> ");
        fgets(comando, MAX_TEXTO, stdin); // fgets es una función para leer archivos texto desde el teclado ( tiene 3 parametros: 1. char *str  que es un puntero a un array de chars que es donde se guarda la info leida, 2. numero maximo de caracteres a leer, 3. File *stream es el flujoj de entrada donde se leerán los datos (stdin indica por ejemplo que se van a leer por teclado)
        comando[strcspn(comando, "\n")] = '\0'; // Eliminar el salto de línea
        // Esta función strcspn tiene dos parametros (1. la cadena a revisar, 2. los caracteres prohibidos) devuelve la posición de un caracter prohibido encontrado. 

        if (strncmp(comando, "join ", 5) == 0) { // Permite comparar caracter por caracter de dos cadenas, mirar si son iguales menor o mayor . En este caso se mira si son iguales
            // Comando para unirse a una sala
            char sala[MAX_NOMBRE];
            sscanf(comando, "join %s", sala); // Comando basico para leer caracteres

            // Enviar solicitud de JOIN al servidor
            msg.mtype = 1; // JOIN
            strcpy(msg.remitente, nombre_usuario);  // Esta función tiene 2 argumentos ( 1. dirección destino, 2. dirección origen) sirve para mover una cadena, copiarla de una dirección a otra de la memoria. 
            strcpy(msg.sala, sala);
            strcpy(msg.texto, "");

            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) { // Esta función sirve para enviar un mensaje a una cola de mensajes (IPC). A continuación los parametros que necesita: 1. identificador de la cola, 2. Un puntero de la estructura a enviar, 3. El tamaño en bytes del campo mtext dentro de la estructura msgbuf, 4. Flags a usar para especificar el comportamiento. 
                perror("Error al enviar solicitud de JOIN"); // Esta función permite imprimir un error descriptivo en C 
                continue;
            }

            // Esperar confirmación del servidor
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) { // Esta función lee un mensaje en una cola de mensajes 
                perror("Error al recibir confirmación");
                continue;
            }

            printf("%s\n", msg.texto);

            // Obtener la cola de la sala
            // key_t key_sala = ftok("/tmp", atoi(sala)); // Esto es un ejemplo, debe ser mejorado
            // Generar misma ruta de archivo para sala
            char ruta_sala[100]; 
            sprintf(ruta_sala, "/tmp/sala_%s", sala);

            // ftok igual que en el servidor
            key_t key_sala = ftok(ruta_sala, 'S'); // Este es el puente que hace que tanto la key del server como la del cliente sean iguales
            cola_sala = msgget(key_sala, 0666);

            cola_sala = msgget(key_sala, 0666);
            if (cola_sala == -1) {
                perror("Error al conectar a la cola de la sala");
                continue;
            }

            strcpy(sala_actual, sala);
        } else if (strlen(comando) > 0) {
            // Enviar un mensaje a la sala actual
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>' para unirte a una.\n");
                continue;
            }

            msg.mtype = 3; // MSG
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, comando);

            if (msgsnd(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje");
                continue;
            }
        }
    }

    return 0;
}
