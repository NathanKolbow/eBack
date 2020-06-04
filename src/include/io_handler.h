#ifndef _IO_HANDLER_H_
#define _IO_HANDLER_H_

#include <sys/types.h>

struct dir_entry {
	char *path; // ABSOLUTE PATH
	char *link;
	unsigned char type; // inherited from respective dirent
	struct dir_entry *next; // next item in the linked list
};

struct dest_lock {
	pthread_mutex_t *lock;
	int len;
	int _curr;
	char **dest;
};

void transfer_init(char *, char **, int, int);
void backup_file(struct dir_entry *);

#endif // _IO_HANDLER_H_