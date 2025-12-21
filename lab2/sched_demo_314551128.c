#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// Structure to hold thread arguments
typedef struct {
    int tid;
    double exec_time;
} thread_arg_t;

// Global variables for policies and priorities
static int *sched;  // 0 for SCHED_NORMAL, 1 for SCHED_FIFO
static int *priorities;

// barrier for threads
static pthread_barrier_t barrier;

// Worker thread functions
static void *thread_func(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    // Wait until all threads are ready
    pthread_barrier_wait(&barrier);
    // Do the task 3 times
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", targ->tid);

        // Busy wait for time_wait seconds using monotonic clock
        struct timespec start, now;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
        double through = 0.0;
        while (through < targ->exec_time) {
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
            through = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
        }
    }
    free(targ);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int num_threads = 0;
    double time_wait = 0.0;
    char *opt_policies = NULL;
    char *opt_priorities = NULL;
    // Parse command-line arguments using getopt
    int opt;
    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch (opt) {
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 't':
                time_wait = atof(optarg);
                break;
            case 's':
                opt_policies = optarg;
                break;
            case 'p':
                opt_priorities = optarg;
                break;
            default:
                fprintf(stderr, "Wrong argument!");
                exit(EXIT_FAILURE);
        }
    }
    // Parse policies and priorities
    sched = malloc(num_threads * sizeof(int));
    priorities = malloc(num_threads * sizeof(int));

    char *token;
    int idx = 0;
    // Parse policies
    char *policies_copy = strdup(opt_policies); // opt_policies is read-only string, and strtok() would modify input, so strdup() is used here
    token = strtok(policies_copy, ",");
    while (token != NULL && idx < num_threads) {
        if (strcmp(token, "NORMAL") == 0) {
            sched[idx] = SCHED_OTHER;  // SCHED_NORMAL is SCHED_OTHER in POSIX
        }
        else if (strcmp(token, "FIFO") == 0) {
            sched[idx] = SCHED_FIFO;
        }
        token = strtok(NULL, ",");
        idx++;
    }
    free(policies_copy);
    policies_copy = NULL;

    // Parse priorities
    char *priorities_copy = strdup(opt_priorities);
    token = strtok(priorities_copy, ",");
    idx = 0;
    while (token != NULL && idx < num_threads) {
        priorities[idx] = atoi(token);
        token = strtok(NULL, ",");
        idx++;
    }
    free(priorities_copy);
    priorities_copy = NULL;

    // Initialize barrier for num_threads + 0 (main doesn't wait)
    pthread_barrier_init(&barrier, NULL, num_threads);

    // Create threads
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        // Initialize thread attributes
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);    // Tell thread not to inherit previous thread's sched, but use the customized sched
        pthread_attr_setschedpolicy(&attr, sched[i]);

        // Set priority if FIFO
        if (sched[i] == SCHED_FIFO) {
            struct sched_param param = {.sched_priority = priorities[i]};
            pthread_attr_setschedparam(&attr, &param);
        } 
        // Set CPU affinity to CPU 0
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
        // Create thread arg
        thread_arg_t *targ = malloc(sizeof(thread_arg_t));
        targ->tid = i;
        targ->exec_time = time_wait;
        // Create thread
        int rc = pthread_create(&threads[i], &attr, thread_func, (void *)targ);
        if (rc) {
            perror("Thread creation error: ");
            exit(EXIT_FAILURE);
        }
        pthread_attr_destroy(&attr);
    }
    // Wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}