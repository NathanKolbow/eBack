#ifndef _SLAVE_H_
#define _SLAVE_H_

#include <pthread.h>

#define DEFAULT_N_SLAVES 10

struct file_lock {
    pthread_mutex_t *lock;
    struct dir_entry *file_list;
};

void spin_slaves(struct file_lock *, int);
void * work_on(void *);
void await_slaves(void);

#endif // _SLAVE_H_