#include <stdio.h>
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

struct mensaje {
    long mtype;
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

struct sala {
    char nombre[MAX_NOMBRE];
    int cola_id;
    int num_usuarios;
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE];
    int mtypes[MAX_USUARIOS_POR_SALA];
};

struct sala salas[MAX_SALAS];
int num_salas = 0;

int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) {
        return -1;
    }

    char ruta_sala[100];
    sprintf(ruta_sala, "/tmp/sala_%s", nombre);

    FILE *fp = fopen(ruta_sala, "a");
    if (fp == NULL) {
        perror("No se pudo crear archivo para sala");
        return -1;
    }
    fclose(fp);

    key_t key = ftok(ruta_sala, 'S');
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;

    printf("Sala creada: %s (cola_id: %d)\n", nombre, cola_id);
    return num_salas++;
}

int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1;
}

void remover_usuario_de_salas(const char *nombre_usuario) {
    for (int i = 0; i < num_salas; i++) {
        for (int j = 0; j < salas[i].num_usuarios; j++) {
            if (strcmp(salas[i].usuarios[j], nombre_usuario) == 0) {
                for (int k = j; k < salas[i].num_usuarios - 1; k++) {
                    strcpy(salas[i].usuarios[k], salas[i].usuarios[k+1]);
                    salas[i].mtypes[k] = salas[i].mtypes[k+1];
                }
                salas[i].num_usuarios--;
                printf("Usuario '%s' removido de la sala '%s'\n", nombre_usuario, salas[i].nombre);
                break;
            }
        }
    }
}

int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return -1;
    }

    struct sala *s = &salas[indice_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) {
        return -1;
    }

    remover_usuario_de_salas(nombre_usuario);

    strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
    s->mtypes[s->num_usuarios] = 100 + s->num_usuarios;
    s->num_usuarios++;

    printf("Usuario '%s' agregado a la sala '%s' (mtype: %d, usuarios totales: %d)\n", 
           nombre_usuario, s->nombre, s->mtypes[s->num_usuarios - 1], s->num_usuarios);
    
    return s->num_usuarios - 1;
}

void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg_original) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return;
    }

    struct sala *s = &salas[indice_sala];
    struct mensaje msg_copia;

    printf("Enviando mensaje a %d usuarios en sala '%s' (remitente: %s)\n", 
           s->num_usuarios, s->nombre, msg_original->remitente);
    
    int mensajes_enviados = 0;
    for (int i = 0; i < s->num_usuarios; i++) {
        // FILTRAR: NO enviar al remitente
        if (strcmp(s->usuarios[i], msg_original->remitente) != 0) {
            msg_copia = *msg_original;
            msg_copia.mtype = s->mtypes[i];
            
            if (msgsnd(s->cola_id, &msg_copia, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje a la sala");
            } else {
                printf("Mensaje enviado a %s (mtype %ld)\n", s->usuarios[i], msg_copia.mtype);
                mensajes_enviados++;
            }
        } else {
            printf("Mensaje NO enviado a %s (es el remitente)\n", s->usuarios[i]);
        }
    }
    printf("Total mensajes enviados: %d (excluyendo al remitente)\n", mensajes_enviados);
}

int main() {
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado. Esperando clientes...\n");
    printf("Cola global creada con key: %d\n", cola_global);

    struct mensaje msg;

    while (1) {
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        printf("Mensaje recibido - Tipo: %ld, Remitente: %s, Sala: %s\n", 
               msg.mtype, msg.remitente, msg.sala);

        if (msg.mtype == 1) { // JOIN
            printf("Solicitud JOIN de %s para sala %s\n", msg.remitente, msg.sala);

            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala == -1) {
                indice_sala = crear_sala(msg.sala);
                if (indice_sala == -1) {
                    printf("Error creando sala %s\n", msg.sala);
                    continue;
                }
            }

            int user_index = agregar_usuario_a_sala(indice_sala, msg.remitente);
            if (user_index != -1) {
                msg.mtype = 2;
                sprintf(msg.texto, "Te has unido a la sala: %s|%d", 
                        msg.sala, salas[indice_sala].mtypes[user_index]);
                
                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar confirmación");
                } else {
                    printf("Confirmación enviada a %s\n", msg.remitente);
                }
            }

        } else if (msg.mtype == 3) { // MSG
            printf("Mensaje de %s en %s: %s\n", msg.remitente, msg.sala, msg.texto);

            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                enviar_a_todos_en_sala(indice_sala, &msg);
            }
        }
    }

    return 0;
}