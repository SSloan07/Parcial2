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

// Códigos de color ANSI para la consola
#define COLOR_RESET   "\033[0m"
#define COLOR_ROJO    "\033[31m"
#define COLOR_VERDE   "\033[32m"
#define COLOR_AMARILLO "\033[33m"
#define COLOR_AZUL    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BLANCO  "\033[37m"

// Colores específicos para nuestra aplicación
#define COLOR_PROPIO    COLOR_VERDE     // Mensajes propios en verde
#define COLOR_EXTERNO   COLOR_CYAN      // Mensajes de otros en cyan
#define COLOR_SISTEMA   COLOR_AMARILLO  // Mensajes del sistema en amarillo
#define COLOR_PROMPT    COLOR_MAGENTA   // Prompt en magenta
#define COLOR_ENTRADA   COLOR_AZUL      // Lo que escribo en azul

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
int hilo_activo = 0;

pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

void *recibir_mensajes(void *arg) {
    struct mensaje msg;
    int cola_actual = *((int*)arg);
    int mtype_actual = mtype_cliente;
    
    while (hilo_activo) {
        if (cola_actual != -1 && mtype_actual != -1) {
            if (msgrcv(cola_actual, &msg, sizeof(struct mensaje) - sizeof(long), mtype_actual, IPC_NOWAIT) == -1) {
                usleep(100000);
                continue;
            }
            
            // Mostrar mensajes EXTERNOS con color cyan
            if (strcmp(msg.remitente, nombre_usuario) != 0) {
                pthread_mutex_lock(&mutex_stdout);
                printf("\n" COLOR_EXTERNO "%s: %s" COLOR_RESET "\n", msg.remitente, msg.texto);
                printf(COLOR_PROMPT "> " COLOR_ENTRADA);
                fflush(stdout);
                pthread_mutex_unlock(&mutex_stdout);
            }
        }
        usleep(100000);
    }
    
    printf(COLOR_SISTEMA "[DEBUG] Hilo receptor terminado" COLOR_RESET "\n");
    return NULL;
}

void cambiar_sala(const char *nueva_sala) {
    struct mensaje msg;
    char sala[MAX_NOMBRE];
    
    hilo_activo = 0;
    usleep(200000);
    
    msg.mtype = 1;
    strcpy(msg.remitente, nombre_usuario);
    strcpy(msg.sala, nueva_sala);
    strcpy(msg.texto, "");

    if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
        perror("Error al enviar solicitud de JOIN");
        return;
    }

    if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
        perror("Error al recibir confirmación");
        return;
    }

    // Mensaje del sistema en amarillo
    printf(COLOR_SISTEMA "%s" COLOR_RESET "\n", msg.texto);

    if (strstr(msg.texto, "Te has unido a la sala:") != NULL) {
        char temp_sala[MAX_NOMBRE];
        if (sscanf(msg.texto, "Te has unido a la sala: %[^|]|%d", temp_sala, &mtype_cliente) == 2) {
            strcpy(sala_actual, temp_sala);
            
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
                printf(COLOR_SISTEMA "[DEBUG] Conectado a la sala '%s' (cola: %d, mtype: %d)" COLOR_RESET "\n", 
                       sala_actual, cola_sala, mtype_cliente);
                
                hilo_activo = 1;
                pthread_t hilo_receptor;
                int result = pthread_create(&hilo_receptor, NULL, recibir_mensajes, &cola_sala);
                if (result != 0) {
                    printf(COLOR_SISTEMA "Error creando hilo receptor" COLOR_RESET "\n");
                    hilo_activo = 0;
                } else {
                    printf(COLOR_SISTEMA "Escuchando mensajes en la sala '%s'..." COLOR_RESET "\n", sala_actual);
                    pthread_detach(hilo_receptor);
                }
            }
        }
    }
}

// Función para mostrar la ayuda con colores
void mostrar_ayuda() {
    printf(COLOR_SISTEMA "\n=== COMANDOS DISPONIBLES ===" COLOR_RESET "\n");
    printf(COLOR_AZUL "join <sala>" COLOR_RESET " - Unirse a una sala (General, Deportes)\n");
    printf(COLOR_AZUL "<mensaje>" COLOR_RESET " - Enviar mensaje a la sala actual\n");
    printf(COLOR_AZUL "exit" COLOR_RESET " - Salir del chat\n");
    printf(COLOR_SISTEMA "=============================" COLOR_RESET "\n\n");
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

    // Banner de bienvenida con colores
    printf(COLOR_SISTEMA "\n=== BIENVENIDO AL CHAT ===" COLOR_RESET "\n");
    printf(COLOR_SISTEMA "Usuario: " COLOR_AZUL "%s" COLOR_RESET "\n", nombre_usuario);
    printf(COLOR_SISTEMA "Salas disponibles: " COLOR_AZUL "General, Deportes" COLOR_RESET "\n");
    printf(COLOR_SISTEMA "Usa " COLOR_AZUL "'join <sala>'" COLOR_SISTEMA " para unirte a una sala" COLOR_RESET "\n");
    printf(COLOR_SISTEMA "Usa " COLOR_AZUL "'help'" COLOR_SISTEMA " para ver los comandos" COLOR_RESET "\n");
    printf(COLOR_SISTEMA "==========================" COLOR_RESET "\n\n");

    struct mensaje msg;
    char comando[MAX_TEXTO];
    pthread_t hilo_receptor = 0;

    while (1) {
        pthread_mutex_lock(&mutex_stdout);
        printf(COLOR_PROMPT "> " COLOR_PROPIO);  // Prompt + color para entrada
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
            
        } else if (strncmp(comando, "help", 4) == 0) {
            mostrar_ayuda();
            
        } else if (strncmp(comando, "exit", 4) == 0) {
            printf(COLOR_SISTEMA "Saliendo del chat..." COLOR_RESET "\n");
            break;
            
        } else if (strlen(comando) > 0) {
            if (strlen(sala_actual) == 0) {
                printf(COLOR_SISTEMA "No estás en ninguna sala. Usa 'join <sala>' para unirte a una." COLOR_RESET "\n");
                continue;
            }

            msg.mtype = 3;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, comando);

            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje");
            } else {
                // Mostrar mensaje PROPIO con color verde (pero NO reescribir)
                pthread_mutex_lock(&mutex_stdout);
                // printf(COLOR_PROPIO "\nTú: %s" COLOR_RESET "\n", comando);
                pthread_mutex_unlock(&mutex_stdout);
            }
        }
    }

    hilo_activo = 0;
    usleep(300000);
    
    printf(COLOR_SISTEMA "Chat finalizado. ¡Hasta pronto!" COLOR_RESET "\n");
    return 0;
}