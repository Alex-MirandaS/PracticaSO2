/* filosofos.c
   Implementación de:
   - modo "naive" : toma tenedor izquierdo y luego intenta derecho (puede producir deadlock)
   - modo "limit" : usa semáforo contador (comedor) con N-1 para evitar deadlock
   Compilar: gcc -pthread filosofos.c -o filosofos
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int N = 5;
sem_t *tenedores;
sem_t comedor;
pthread_mutex_t mutex_print;
int *filosofos_comiendo;
int *total_comidas;
int *holding_left;
int running = 1;
time_t end_time;
char mode[16] = "naive";

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
    unsigned int seed = time(NULL) ^ (id << 8);

    while (time(NULL) < end_time) {
        long tthink = rand_range( (long) 1000000, (long) 2000000 );
        msleep(tthink);
        safe_print("[P%d] Quiere comer. Pensó %ld us.\n", id, tthink);

        if (sem_wait(&tenedores[left]) == 0) {
            holding_left[id] = 1;
            safe_print("[P%d] Tomó tenedor IZQ (%d).\n", id, left);
        } else {
            safe_print("[P%d] Error al tomar tenedor izquierdo: %s\n", id, strerror(errno));
        }

        while (time(NULL) < end_time) {
            if (sem_trywait(&tenedores[right]) == 0) {

                holding_left[id] = 0;
                filosofos_comiendo[id] = 1;
                total_comidas[id]++;
                safe_print("[P%d] Tomó tenedor DER (%d). EMPIEZA A COMER (total=%d).\n", id, right, total_comidas[id]);
                long te = rand_range((long)500000, (long)1500000);
                msleep(te);
                safe_print("[P%d] TERMINÓ de comer tras %ld us.\n", id, te);
                filosofos_comiendo[id] = 0;

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
            safe_print("[P%d] Libera izquierdo por tiempo agotado o salida.\n", id);
        }
    }

    free(arg);
    return NULL;
}

void *filosofo_limit(void *arg) {
    int id = *(int*)arg;
    int left = id;
    int right = (id + 1) % N;
    unsigned int seed = time(NULL) ^ (id << 8);

    while (time(NULL) < end_time) {
        long tthink = rand_range( (long) 1000000, (long) 2000000 );
        msleep(tthink);
        safe_print("[P%d] Quiere comer. Pensó %ld us.\n", id, tthink);

        sem_wait(&comedor);
        sem_wait(&tenedores[left]);
        sem_wait(&tenedores[right]);

        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        safe_print("[P%d] Tomó IZQ(%d) y DER(%d). EMPIEZA A COMER (total=%d).\n", id, left, right, total_comidas[id]);
        long te = rand_range((long)500000, (long)1500000);
        msleep(te);
        safe_print("[P%d] TERMINÓ de comer tras %ld us.\n", id, te);
        filosofos_comiendo[id] = 0;

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
            int all_holding_left = 1;
            int any_eating = 0;
            for (int i = 0; i < N; ++i) {
                if (!holding_left[i]) { all_holding_left = 0; break; }
                if (filosofos_comiendo[i]) any_eating = 1;
            }
            if (all_holding_left && !any_eating && !reported) {
                safe_print("\n>>> DEADLOCK DETECTADO: todos los filosofos han tomado su tenedor IZQ y ninguno puede tomar DER.\n\n");
                reported = 1;
            }
        } else {
        }
    }
    return NULL;
}

void print_summary() {
    safe_print("\n--- RESUMEN: total comidas por filósofo ---\n");
    for (int i = 0; i < N; ++i) {
        safe_print("Filósofo %d : %d comidas\n", i, total_comidas[i]);
    }
}

int main(int argc, char *argv[]) {

    long THINK_MIN = TIEMPO_PENSAR_MIN;
    long THINK_MAX = TIEMPO_PENSAR_MAX;
    long EAT_MIN   = TIEMPO_COMER_MIN;
    long EAT_MAX   = TIEMPO_COMER_MAX;
    int SIM_SECONDS = TIEMPO_SIMULACION;

    if (argc >= 2) strncpy(mode, argv[1], sizeof(mode)-1);
    if (argc >= 3) N = atoi(argv[2]);
    if (argc >= 4) THINK_MIN = atol(argv[3]);
    if (argc >= 5) THINK_MAX = atol(argv[4]);
    if (argc >= 6) EAT_MIN = atol(argv[5]);
    if (argc >= 7) EAT_MAX = atol(argv[6]);
    if (argc >= 8) SIM_SECONDS = atoi(argv[7]);
    int comedor_limit = (argc >= 9) ? atoi(argv[8]) : (N>1 ? N-1 : 1);

    printf("Modo: %s | N=%d | THINK[%ld-%ld]us | EAT[%ld-%ld]us | SIM=%ds | comedor_limit=%d\n",
           mode, N, THINK_MIN, THINK_MAX, EAT_MIN, EAT_MAX, SIM_SECONDS, comedor_limit);

    srand(time(NULL));

    tenedores = malloc(sizeof(sem_t) * N);
    filosofos_comiendo = calloc(N, sizeof(int));
    total_comidas = calloc(N, sizeof(int));
    holding_left = calloc(N, sizeof(int));
    pthread_mutex_init(&mutex_print, NULL);

    for (int i = 0; i < N; ++i) sem_init(&tenedores[i], 0, 1);
    sem_init(&comedor, 0, comedor_limit);

    pthread_t *threads = malloc(sizeof(pthread_t) * N);
    pthread_t monitor;

    end_time = time(NULL) + SIM_SECONDS;

    for (int i = 0; i < N; ++i) {
        int *id = malloc(sizeof(int));
        *id = i;
        if (strcmp(mode, "limit") == 0) {
            pthread_create(&threads[i], NULL, filosofo_limit, id);
        } else {
            pthread_create(&threads[i], NULL, filosofo_naive, id);
        }
    }

    pthread_create(&monitor, NULL, monitor_deadlock, NULL);


    for (int i = 0; i < N; ++i) pthread_join(threads[i], NULL);
    pthread_join(monitor, NULL);

    print_summary();


    for (int i = 0; i < N; ++i) sem_destroy(&tenedores[i]);
    sem_destroy(&comedor);
    pthread_mutex_destroy(&mutex_print);
    free(tenedores);
    free(threads);
    free(filosofos_comiendo);
    free(total_comidas);
    free(holding_left);

    return 0;
}
