#ifndef _IO_HANDLER_H_
#define _IO_HANDLER_H_

#include <sys/types.h>

struct dir_entry {
	struct dir_entry *next; // next item in the linked list
	
	mode_t st_mode;
	unsigned long int mtim_tv_sec;
	unsigned long int atim_tv_sec;
	off_t st_size;
	int stat_err;

	char *path; // ABSOLUTE PATH
	char *link;
	unsigned char type; // inherited from respective dirent
};

struct dest_lock {
	pthread_mutex_t *lock;
	int len;
	int _curr;
	char **dest;
};

void transfer_init(char *, char **, int, int);
void backup_file(struct dir_entry *);
void free_list_node(struct dir_entry *);

#endif // _IO_HANDLER_H_