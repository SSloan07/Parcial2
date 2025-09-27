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
    long mtype;
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

int cola_global;
int cola_sala = -1;
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";
int mtype_cliente = -1;
int hilo_activo = 0; // Bandera para controlar el hilo receptor

pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_receptor = PTHREAD_MUTEX_INITIALIZER;

void *recibir_mensajes(void *arg) {
    struct mensaje msg;
    int cola_actual = *((int*)arg);
    int mtype_actual = mtype_cliente;
    
    // printf("[DEBUG] Hilo receptor iniciado - Cola: %d, mtype: %d\n", cola_actual, mtype_actual); Esto es solo un mensaje de debug
    
    while (hilo_activo) {
        if (cola_actual != -1 && mtype_actual != -1) {
            // Recibir mensajes específicos para este cliente
            if (msgrcv(cola_actual, &msg, sizeof(struct mensaje) - sizeof(long), mtype_actual, IPC_NOWAIT) == -1) {
                // No hay mensajes, esperar un poco
                usleep(100000);
                continue;
            }
            
            // Solo mostrar mensajes que no sean del propio usuario
            if (strcmp(msg.remitente, nombre_usuario) != 0) {
                pthread_mutex_lock(&mutex_stdout);
                printf("\n%s: %s\n", msg.remitente, msg.texto);
                printf("> ");
                fflush(stdout);
                pthread_mutex_unlock(&mutex_stdout);
            }
        }
        usleep(100000);
    }
    
    printf("[DEBUG] Hilo receptor terminado\n");
    return NULL;
}

void cambiar_sala(const char *nueva_sala) {
    struct mensaje msg;
    char sala[MAX_NOMBRE];
    
    // Detener el hilo receptor actual
    hilo_activo = 0;
    usleep(200000); // Dar tiempo al hilo para terminar
    
    // Enviar solicitud JOIN al servidor para la nueva sala
    msg.mtype = 1;
    strcpy(msg.remitente, nombre_usuario);
    strcpy(msg.sala, nueva_sala);
    strcpy(msg.texto, "");

    if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
        perror("Error al enviar solicitud de JOIN");
        return;
    }

    // Esperar confirmación del servidor
    if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
        perror("Error al recibir confirmación");
        return;
    }

    printf("%s\n", msg.texto);

    // Parsear la confirmación para obtener el mtype único
    if (strstr(msg.texto, "Te has unido a la sala:") != NULL) {
        char temp_sala[MAX_NOMBRE];
        if (sscanf(msg.texto, "Te has unido a la sala: %[^|]|%d", temp_sala, &mtype_cliente) == 2) {
            strcpy(sala_actual, temp_sala);
            
            // Conectar a la cola de la nueva sala
            char ruta_sala[100];
            sprintf(ruta_sala, "/tmp/sala_%s", sala_actual);
            key_t key_sala = ftok(ruta_sala, 'S');
            int nueva_cola_sala = msgget(key_sala, 0666);
            
            if (nueva_cola_sala == -1) {
                perror("Error al conectar a la cola de la sala");
                cola_sala = -1;
                mtype_cliente = -1;
            } else {
                cola_sala = nueva_cola_sala;
                printf("[DEBUG] Conectado a la sala '%s' (cola: %d, mtype: %d)\n", 
                       sala_actual, cola_sala, mtype_cliente);
                
                // Reiniciar el hilo receptor con la nueva sala
                hilo_activo = 1;
                pthread_t hilo_receptor;
                int result = pthread_create(&hilo_receptor, NULL, recibir_mensajes, &cola_sala);
                if (result != 0) {
                    printf("Error creando hilo receptor\n");
                    hilo_activo = 0;
                } else {
                    printf("Escuchando mensajes en la sala '%s'...\n", sala_actual);
                    pthread_detach(hilo_receptor); // El hilo se limpiará automáticamente al terminar
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }

    strcpy(nombre_usuario, argv[1]);

    // Conectarse a la cola global
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    printf("Bienvenido, %s. Salas disponibles: General, Deportes\n", nombre_usuario);
    printf("Usa 'join <sala>' para unirte a una sala.\n");

    struct mensaje msg;
    char comando[MAX_TEXTO];
    pthread_t hilo_receptor = 0;

    while (1) {
        pthread_mutex_lock(&mutex_stdout);
        printf("> ");
        fflush(stdout);
        pthread_mutex_unlock(&mutex_stdout);

        if (!fgets(comando, MAX_TEXTO, stdin)) {
            break;
        }

        comando[strcspn(comando, "\n")] = '\0';

        if (strncmp(comando, "join ", 5) == 0) {
            char sala[MAX_NOMBRE];
            sscanf(comando, "join %s", sala);
            
            cambiar_sala(sala);
            
        } else if (strlen(comando) > 0) {
            // Enviar mensaje a la sala actual
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>' para unirte a una.\n");
                continue;
            }

            msg.mtype = 3;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, comando);

            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje");
            } else {
                // Mostrar nuestro propio mensaje localmente
                pthread_mutex_lock(&mutex_stdout);
                printf("Tú: %s\n", comando);
                pthread_mutex_unlock(&mutex_stdout);
            }
        }
    }

    // Limpiar antes de salir
    hilo_activo = 0;
    usleep(300000);
    
    return 0;
}