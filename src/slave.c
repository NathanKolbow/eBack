#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "io_handler.h"
#include "slave.h"
#include "debug.h"

pthread_t *SLAVE_IDS;
int N_SLAVES;

// Starts the slaves
void spin_slaves(struct file_lock *lock, int n_slaves) {
    N_SLAVES = n_slaves;
    if(n_slaves < 1) {
        fprintf(stderr, "Warning: spin_slaves(...) called with %d slaves, running with %d instead.\n", n_slaves, DEFAULT_N_SLAVES);
        N_SLAVES = DEFAULT_N_SLAVES;
    }

    SLAVE_IDS = malloc(sizeof(pthread_t) * N_SLAVES);
    if(SLAVE_IDS == NULL) {
        fprintf(stderr, "Error: Failed to malloc in spin_slaves(...).\n");
        exit(-1);
    }

	for(int i = 0; i < N_SLAVES; i++) {
		pthread_create(&SLAVE_IDS[i], NULL, &work_on, lock);
	}
}

// Waits for slaves to finish
void await_slaves() {
    if(SLAVE_IDS == NULL) {
        fprintf(stderr, "Error: Cannot wait for slaves when there aren't any slaves to wait for.\n");
        return;
    }

    for(int i = 0; i < N_SLAVES; i++) {
		pthread_join(SLAVE_IDS[i], NULL);
	}
}

// the lock in lock must already be initialized
void * work_on(void *arg) {
    char msg[128];
    sprintf(msg, "Thread %lu has begun work.\n", pthread_self());
    d_log(DB_DEBUG, msg);

    struct file_lock *lock = arg;
    struct dir_entry *ent = NULL;

    while(1) {
        // Lock the list, get the next file and move the list along, then release the lock
        pthread_mutex_lock(lock->lock);
        ent = lock->file_list;
        if(ent == NULL) {
            pthread_mutex_unlock(lock->lock);
            break;
        }

        lock->file_list = lock->file_list->next;
        pthread_mutex_unlock(lock->lock);

        backup_file(ent);
    }

    msg[0] = '\0';
    sprintf(msg, "Thread %lu has finished work.\n", pthread_self());
    d_log(DB_DEBUG, msg);
    
    return NULL;
}