#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
// Definimos los valores predefinidos
#define TIEMPO_PENSAR_MIN 1000000
#define TIEMPO_PENSAR_MAX 3000000
#define TIEMPO_COMER_MIN  500000
#define TIEMPO_COMER_MAX 1500000
#define TIEMPO_SIMULACION 30

// Variables globales
int N;
sem_t *tenedores;
sem_t comedor;
pthread_mutex_t mutex_print;
int *filosofos_comiendo;
int *total_comidas;
int *holding_left;
time_t end_time;
char mode[16];

long rand_range(long min, long max) {
    if (max <= min) return min;
    return min + (rand() % (max - min + 1));
}

void msleep(long microseconds) {
    usleep((useconds_t)microseconds);
}

void safe_print(const char *fmt, ...) {
    va_list ap;
    pthread_mutex_lock(&mutex_print);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    pthread_mutex_unlock(&mutex_print);
}


void *filosofo_naive(void *arg) {
    int id = *(int*)arg;
    int left = id;
    int right = (id + 1) % N;

    while (time(NULL) < end_time) {
        long tthink = rand_range(100000, 900000);
        msleep(tthink);
        safe_print("[P%d] Quiere comer. Pensó %ld us.\n", id, tthink);

        sem_wait(&tenedores[left]);
        holding_left[id] = 1;
        safe_print("[P%d] Tomó tenedor IZQ (%d).\n", id, left);

        while (time(NULL) < end_time) {
            if (sem_trywait(&tenedores[right]) == 0) {
                holding_left[id] = 0;
                filosofos_comiendo[id] = 1;
                total_comidas[id]++;
                safe_print("[P%d] Tomó tenedor DER (%d). COMIENDO (total=%d)\n", id, right, total_comidas[id]);
                long te = rand_range(2000000, 3000000);
                msleep(te);
                filosofos_comiendo[id] = 0;
                safe_print("[P%d] Terminó de comer (%ld us).\n", id, te);
                sem_post(&tenedores[right]);
                sem_post(&tenedores[left]);
                break;
            } else {
                msleep(100000);
            }
        }

        if (holding_left[id]) {
            sem_post(&tenedores[left]);
            holding_left[id] = 0;
        }
    }

    free(arg);
    return NULL;
}

void *filosofo_limit(void *arg) {
    int id = *(int*)arg;
    int left = id;
    int right = (id + 1) % N;

    while (time(NULL) < end_time) {
        long tthink = rand_range(TIEMPO_PENSAR_MIN, TIEMPO_PENSAR_MAX);
        msleep(tthink);
        safe_print("[P%d] Quiere comer. Pensó %ld us.\n", id, tthink);

        sem_wait(&comedor);
        sem_wait(&tenedores[left]);
        sem_wait(&tenedores[right]);

        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        safe_print("[P%d] Tomó IZQ(%d) y DER(%d). COMIENDO (total=%d)\n", id, left, right, total_comidas[id]);
        long te = rand_range(TIEMPO_COMER_MIN, TIEMPO_COMER_MAX);
        msleep(te);
        filosofos_comiendo[id] = 0;
        safe_print("[P%d] Terminó de comer (%ld us).\n", id, te);

        sem_post(&tenedores[right]);
        sem_post(&tenedores[left]);
        sem_post(&comedor);
    }

    free(arg);
    return NULL;
}

void *monitor_deadlock(void *arg) {
    int reported = 0;
    while (time(NULL) < end_time) {
        msleep(300000);
        if (strcmp(mode, "naive") == 0) {
            int all_holding = 1;
            int any_eating = 0;
            for (int i = 0; i < N; i++) {
                if (!holding_left[i]) all_holding = 0;
                if (filosofos_comiendo[i]) any_eating = 1;
            }
            if (all_holding && !any_eating && !reported) {
                safe_print("\n>>> DEADLOCK DETECTADO: todos los filósofos tienen tenedor izquierdo y nadie come.\n\n");
                reported = 1;
            }
        }
    }
    return NULL;
}

void print_summary() {
    safe_print("\n--- RESUMEN DE COMIDAS ---\n");
    for (int i = 0; i < N; i++) {
        safe_print("Filósofo %d : %d comidas\n", i, total_comidas[i]);
    }
}

//CONFIGURAMOS LOS ESCENARIOS DESDE CONSOLA.
void configurar_escenario(int escenario) {
    switch (escenario) {
        case 1:
            N = 5;
            strcpy(mode, "naive");
            printf("Escenario 1: Interbloqueo\n");
            printf("Configuración: 5 filósofos, pensar <1s, comer 2-3s, duración 10s\n");
            break;
        case 2:
            N = 5;
            strcpy(mode, "limit");
            printf("Escenario 2: Equidad\n");
            printf("Configuración: 5 filósofos, pensar 1-2s, comer 0.5-1s, duración 60s\n");
            break;
        case 3:
            N = 3;
            strcpy(mode, "limit");
            printf("Escenario 3: Robustez\n");
            printf("Configuración: 3 filósofos, pensar 50-100ms, comer 100-300ms, duración 30s\n");
            break;
        case 4:
            strcpy(mode, "limit");
            printf("Escenario 4: Variables (2,7,12 filósofos), pensar y comer estándar, 20s cada uno\n");
            break;
        default:
            printf("Escenario no válido.\n");
            exit(1);
    }
}

int main() {
    int escenario;
    printf("Seleccione el escenario (1-4): ");
    scanf("%d", &escenario);
    configurar_escenario(escenario);

    srand(time(NULL));
    pthread_mutex_init(&mutex_print, NULL);

    if (escenario == 4) {
        int escenarios[] = {2, 7, 12};
        for (int e = 0; e < 3; e++) {
            N = escenarios[e];
            printf("\n--- Ejecutando con %d filósofos ---\n", N);

            tenedores = malloc(sizeof(sem_t) * N);
            filosofos_comiendo = calloc(N, sizeof(int));
            total_comidas = calloc(N, sizeof(int));
            holding_left = calloc(N, sizeof(int));

            for (int i = 0; i < N; i++) sem_init(&tenedores[i], 0, 1);
            sem_init(&comedor, 0, N - 1);
            end_time = time(NULL) + 20;

            pthread_t threads[N], monitor;
            for (int i = 0; i < N; i++) {
                int *id = malloc(sizeof(int));
                *id = i;
                pthread_create(&threads[i], NULL, filosofo_limit, id);
            }
            pthread_create(&monitor, NULL, monitor_deadlock, NULL);

            for (int i = 0; i < N; i++) pthread_join(threads[i], NULL);
            pthread_join(monitor, NULL);

            print_summary();

            for (int i = 0; i < N; i++) sem_destroy(&tenedores[i]);
            sem_destroy(&comedor);
            free(tenedores);
            free(filosofos_comiendo);
            free(total_comidas);
            free(holding_left);
        }
        pthread_mutex_destroy(&mutex_print);
        return 0;
    }

    if (escenario == 1) {
        N = 5;
        tenedores = malloc(sizeof(sem_t) * N);
        filosofos_comiendo = calloc(N, sizeof(int));
        total_comidas = calloc(N, sizeof(int));
        holding_left = calloc(N, sizeof(int));
        for (int i = 0; i < N; i++) sem_init(&tenedores[i], 0, 1);
        sem_init(&comedor, 0, N - 1);
        end_time = time(NULL) + 10;
    } else if (escenario == 2) {
        N = 5;
        tenedores = malloc(sizeof(sem_t) * N);
        filosofos_comiendo = calloc(N, sizeof(int));
        total_comidas = calloc(N, sizeof(int));
        holding_left = calloc(N, sizeof(int));
        for (int i = 0; i < N; i++) sem_init(&tenedores[i], 0, 1);
        sem_init(&comedor, 0, 4);
        end_time = time(NULL) + 60;
    } else if (escenario == 3) {
        N = 3;
        tenedores = malloc(sizeof(sem_t) * N);
        filosofos_comiendo = calloc(N, sizeof(int));
        total_comidas = calloc(N, sizeof(int));
        holding_left = calloc(N, sizeof(int));
        for (int i = 0; i < N; i++) sem_init(&tenedores[i], 0, 1);
        sem_init(&comedor, 0, 2);
        end_time = time(NULL) + 30;
    }

    pthread_t threads[N], monitor;
    for (int i = 0; i < N; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        if (strcmp(mode, "naive") == 0)
            pthread_create(&threads[i], NULL, filosofo_naive, id);
        else
            pthread_create(&threads[i], NULL, filosofo_limit, id);
    }

    pthread_create(&monitor, NULL, monitor_deadlock, NULL);

    for (int i = 0; i < N; i++) pthread_join(threads[i], NULL);
    pthread_join(monitor, NULL);

    print_summary();

    for (int i = 0; i < N; i++) sem_destroy(&tenedores[i]);
    sem_destroy(&comedor);
    pthread_mutex_destroy(&mutex_print);
    free(tenedores);
    free(filosofos_comiendo);
    free(total_comidas);
    free(holding_left);

    return 0;
}
