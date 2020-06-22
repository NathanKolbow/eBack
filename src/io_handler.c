#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#include "back_util.h"
#include "btrfs_handler.h"
#include "io_handler.h"
#include "slave.h"
#include "debug.h"

struct dir_entry * collectInit(char *);
dev_t getDeviceID(char *);
int create_link(char *, char *);
int try_backup(struct dir_entry *, char *, char);
int mkdir_recursive(char *, int);
void freeFileList(struct dir_entry *);
int sameDevice(dev_t, dev_t, char *);
struct dir_entry * collectFileList(char *, struct dir_entry *, dev_t);
int rnd();
int recursive_dir(char *);

char **GLOBAL_SRC_LIST;
int GLOBAL_SRC_SIZE;

char **DEST;
pthread_mutex_t *DEST_LOCKS;
int LOCK_LEN;
char *SRC_PREFIX;

mode_t MODE_MASK = S_IRWXU | S_IRWXG | S_IRWXO;

// Begins and then manages the transference of files from src to dest
//    --> char **dest is a LIST of destinations, starting from the first
//        and only moving to the second once the first is full
//        HOWEVER, all destinations are checked for a pre-existing, identical
//        file before a file is backed up
//
// Global var SRC_LOCK should ALREADY be initialized (lock and all)
void transfer_init(char *src, char **dest, int len, int n_slaves) {
	FILE *dir_list_fp = fopen(dirlist_loc, "r");
	if(dir_list_fp == NULL) {
		fprintf(stderr, "Error: Unable to open %s.\n", dirlist_loc);
		exit(-1);
	}
	char buffer[1024];
	int size = 0;
	while(fgets(buffer, sizeof(buffer), dir_list_fp) != NULL)
		size++;
	rewind(dir_list_fp);

	GLOBAL_SRC_LIST = malloc(sizeof(char *) * size);
	GLOBAL_SRC_SIZE = size;
	size = 0;
	while(fgets(buffer, sizeof(buffer), dir_list_fp) != NULL) {
		fix_newline(buffer);

		GLOBAL_SRC_LIST[size] = malloc(strlen(buffer) + 1);
		strcpy(GLOBAL_SRC_LIST[size], buffer);
		size++;
	}


	SRC_PREFIX = src;
	DEST = dest;

	DEST_LOCKS = malloc(len * sizeof(pthread_mutex_t));
	for(int i = 0; i < len; i++)
		pthread_mutex_init(DEST_LOCKS+i, NULL);
	LOCK_LEN = len;

	time_t t;
	srand((unsigned) time(&t));

	char msg[64];
	sprintf(msg, "Collecting file list for %s.\n", src);
	d_log(DB_DEBUG, msg);

	struct dir_entry *list = collectInit(src); 
	struct file_lock *lock = malloc(sizeof(struct file_lock));
	lock->lock = malloc(sizeof(pthread_mutex_t));
	lock->file_list = list;
	pthread_mutex_init(lock->lock, NULL);

	msg[0] = '\0';
	sprintf(msg, "Spinning slaves.\n");
	d_log(DB_DEBUG, msg);

	spin_slaves(lock, n_slaves);
	await_slaves();

	freeFileList(list);
}

// Tries to create a backup of ent in one of the valid dests in DEST_LOCK
void backup_file(struct dir_entry *ent) {
	// first check if dest already exists
	if(ent->type == DT_REG && ent->stat_err != 0) {
		char msg[1024];
		sprintf(msg, "Error: Failed to retrieve file info for %s, error #%d: %s\n", ent->path, ent->stat_err, strerror(ent->stat_err));
		d_log(DB_WARNING, msg);
		return;
	}

	// First we look to see if the backup already exists
	char *dest;
	int i;
	for(i = 0; i < LOCK_LEN; i++) {
		dest = malloc(strlen(DEST[i]) + strlen(ent->path) + 1); // the room for SRC_PREFIX is built into ent->path
		sprintf(dest, "%s%s%s", DEST[i], SRC_PREFIX, ent->path+strlen(SRC_PREFIX));

		struct stat dest_info;
		if(stat(dest, &dest_info) == -1) {
			// file does not already exist here, try the next location
			free(dest);
			dest = NULL; // NULL out dest in case this is the last dir we're checking
			continue;
		}

		if(ent->mtim_tv_sec == dest_info.st_mtim.tv_sec && ent->mtim_tv_sec != 0) {
			// file has an identical match already
			char msg[1024];
			sprintf(msg, "%s is the same version as %s (%lu == %lu); skipping.\n", ent->path, dest, ent->mtim_tv_sec, dest_info.st_mtim.tv_sec);
			d_log(DB_DEBUG, msg);

			free(dest);
			// we can just leave
			return;
		}

		char msg[1024];
		sprintf(msg, "Found an old version of %s in %s!\n", ent->path, dest);
		d_log(DB_EVERYTHING, msg);
		break; // If we got all the way out here, that means we found an old copy!
	}



	// Once we have concluded that the file is not already backed up, we try and back it up
	if(dest == NULL) {
		i = rnd();
	} else {
		// if we got here AND dest is not NULL, then dest is an old copy of our file, so we simply unlink it
		unlink(dest);
	}

	int count = 0;
	// Loops through each dest, trying them one by one
	while(count < LOCK_LEN) {
		if(DEST[i] == NULL) {
			free(dest);
			count++;
			i = (i+1) % LOCK_LEN;
			continue;
		}

		dest = malloc(strlen(DEST[i]) + strlen(ent->path) + 1); // the room for SRC_PREFIX is built into ent->path
		sprintf(dest, "%s%s%s", DEST[i], SRC_PREFIX, ent->path+strlen(SRC_PREFIX));

		// Now we just try backing it up on each loop!  If we succeed, we leave.
		if(try_backup(ent, dest, 0)) {
			char msg[1024];
			sprintf(msg, "Successfully backed up %s in %s.\n", ent->path, dest);
			d_log(DB_EVERYTHING, msg);

			free(dest);
			return;
		} else {
			// If the drive has less than DRIVE_CUTOFF_CAPACITY memory remaining, take it out of the list
			struct statvfs buf;
			statvfs(DEST[i], &buf);

			if(buf.f_bavail * buf.f_bsize < DRIVE_CUTOFF_CAPACITY) {
				char msg[1024];
				sprintf(msg, "Fatal: Ran out of space on %s; %lu, %lu, %lu < %d.\n", DEST[i], buf.f_bavail, buf.f_bsize, buf.f_bavail*buf.f_bsize, DRIVE_CUTOFF_CAPACITY);
				d_log(DB_FATAL, msg);

				DEST[i] = NULL;
			}
		}


		free(dest);
		count++;
		i = (i+1) % LOCK_LEN;
	}

	for(int i = 0; i < LOCK_LEN; i++)
		if(DEST[i] != NULL)
			return;

	// all destinations are FULL (down to DRIVE_CUTOFF_CAPACITY granularity)
	char msg[1024];
	sprintf(msg, "Fatal: No more space remaining.\n");
	d_log(DB_FATAL, msg);

	exit(-1);
}

int make_parents(char *path) {
	int _in = strlen(path);
	while(_in >= 1) {
		if(path[_in] == '/') {
			char save = path[_in+1];
			path[_in+1] = '\0';
			int ret = mkdir_recursive(path, 0777);
			path[_in+1] = save;
			return ret;
		}

		_in--;
	}
	return 0;
}

// Tries to backup src to dest.  If successful, returns 1, else returns 0
// For use as a helper function
//
// src and dest must BOTH be ABSOLUTE FILE PATHS
int try_backup(struct dir_entry *ent, char *dest, char depth) {
	if(ent->type == DT_REG) {
		int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, ent->st_mode);
		if(dest_fd == -1) {
			if(depth == 0) {
				if(make_parents(dest) == 1) {
					// retry if the file's parent dir didn't exist but was successfully created here
					return try_backup(ent, dest, 1);
				}
			}

			char msg[1024];
			sprintf(msg, "Error: Could not open %s; skipping.\n", dest);
			d_log(DB_FATAL, msg);
			return 0;
		}

		int src_fd = open(ent->path, O_RDONLY);
		if(src_fd == -1) {
			char msg[1024];
			sprintf(msg, "Error: Could not open %s; skipping.\n", ent->path);
			d_log(DB_FATAL, msg);

			close(dest_fd);
			return 0;
		}

		off_t offset = 0;
		printf("Copying file %s to %s\n", ent->path, dest);
		ssize_t bytes_sent = sendfile(dest_fd, src_fd, &offset, ent->st_size);
		fchmod(dest_fd, ent->st_mode);

		close(src_fd);
		close(dest_fd);

		if(bytes_sent != ent->st_size) {
			unlink(dest);
			return 0;
		} else {
			char msg[1024];
			sprintf(msg, "Created %s.\n", dest);
			d_log(DB_EVERYTHING, msg);

			struct utimbuf src_time_data;
			src_time_data.actime = ent->atim_tv_sec;
			src_time_data.modtime = ent->mtim_tv_sec;

			utime(dest, &src_time_data);
			return 1;
		}
	} else if(ent->type == DT_LNK) {
		return create_link(ent->link, dest);
	} else if(ent->type == DT_DIR) {
		int ret = mkdir_recursive(dest, ent->st_mode);
		if(ret == 0) {
			char msg[1024]; 
			sprintf(msg, "Fatal: Could not make directory %s due to error #%d: %s.\n", dest, errno, strerror(errno));
			d_log(DB_FATAL, msg);
			
			return 0;
		}
		return 1;
	} else {
		char msg[1024];
		sprintf(msg, "Warning: Received unknown file type (%d) in try_backup(...) for file %s.\n", ent->type, ent->path);
		d_log(DB_WARNING, msg);

		return 0;
	}
}

int mkdir_recursive(char *dir, int mode) {
	char tmp[256];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp),"%s",dir);
	len = strlen(tmp);
	if(tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for(p = tmp + 1; *p; p++)
		if(*p == '/') {
			*p = 0;
			if(mkdir(tmp, mode) == -1 && errno != EEXIST) {
				char msg[1024];
				sprintf(msg, "Fatal: Could not make directory %s; #%d: %s.\n", dir, errno, strerror(errno));
				d_log(DB_FATAL, msg);
				return 0;
			}
			*p = '/';
		}
	if(mkdir(tmp, mode) == -1 && errno != EEXIST) {
		char msg[1024];
		sprintf(msg, "Fatal: Could not make directory %s; #%d: %s.\n", dir, errno, strerror(errno));
		d_log(DB_FATAL, msg);
		return 0;
	}

	return 1;
}

// Creates a symbolic link named link2 that points to link1
// link1 does NOT need to be a valid file/folder
int create_link(char *target, char *new) {
	int ret = symlink(target, new);
	if(ret == -1) {
		if(errno == 17) // file already exists, this can be safely ignored
			return 1;

		char msg[1024];
		sprintf(msg, "Warning: Could not create symbolic link %s --> %s, error #%d: %s.\n", new, target, errno, strerror(errno));
		d_log(DB_WARNING, msg);
		return 0;
	}

	char msg[1024];
	sprintf(msg, "Created link %s --> %s.\n", new, target);
	d_log(DB_EVERYTHING, msg);

	return 1;
}



// Initializer function for file list collection
// Also does initiation for btrfs configurations
struct dir_entry * collectInit(char *root) {
	if(isBtrfs(root)) btrfs_init(root);

	dev_t device = getDeviceID(root);

	return collectFileList(root, NULL, device);
}

// first should be treated as the working directory, second should be treated
// as the directory we are entering
int sameDevice(dev_t first, dev_t second, char *new_path) {
	return first == second || (btrfs() && btrfsSameDevice(first, second, new_path));
}

dev_t getDeviceID(char *path) {
    struct stat info;
    stat(path, &info);
    return info.st_dev;
}

// This collects RECURSIVELY starting from a root directory
// By convention, the char *root var should ALWAYS end with a '/'
// Final list is stored in the list argument
//
// NOTE: Links that are ALSO directories are NEVER called upon recursively
struct dir_entry * collectFileList(char *root, struct dir_entry *list, dev_t device) {
	DIR *curr_dir = opendir(root);
	struct dirent *entry;

	if(curr_dir) {
		while((entry = readdir(curr_dir)) != NULL) {
			// skip infinite loops
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;

			// all that we care about keeping is directories, files, and links, so:
			if(entry->d_type == DT_REG || entry->d_type == DT_DIR || entry->d_type == DT_LNK) {
				struct dir_entry *new = malloc(sizeof(struct dir_entry));
				struct stat info;

				new->type = entry->d_type;
				new->path = malloc((strlen(root) + strlen(entry->d_name) + 1));
				new->path[0] = '\0';
				sprintf(new->path, "%s%s", root, entry->d_name);
				new->path[strlen(root)+strlen(entry->d_name)] = '\0';
				new->next = NULL;
				new->link = NULL;
				
				new->stat_err = (stat(new->path, &info) == 0) ? 0 : errno;
				new->atim_tv_sec = info.st_atim.tv_sec;
				new->mtim_tv_sec = info.st_mtim.tv_sec;
				new->st_mode = MODE_MASK & info.st_mode;
				new->st_size = info.st_size;


				if(entry->d_type == DT_LNK) {
					// first find where the link is pointing
					char buffer[1024];
					int len = readlink(new->path, buffer, 1024);

					if(len >= 1024) {
						fprintf(stderr, "Error: Failed to follow the the link created by %s.\n", new->path);
					} else {
						// readlink does NOT null terminate the string for us *eye roll*
						buffer[len] = 0;
						// change dir so that realpath works properly for relative paths
						chdir(root);
						
						char *resolved = realpath(buffer, NULL);
						new->link = resolved;
					}
				}

				if(entry->d_type == DT_DIR) {
					// first construct the folder's absolute path
					char *newDir = malloc((strlen(root) + strlen(entry->d_name) + 2));
					sprintf(newDir, "%s%s/", root, entry->d_name);
					newDir[strlen(root) + strlen(entry->d_name) + 1] = '\0';
					
					// check if the folder is in the same filesystem and only continue if it is
					if(!sameDevice(getDeviceID(root), getDeviceID(newDir), newDir)) {
						fprintf(stderr, "SKIPPING %s as it is apart of a different filesystem.\n", newDir);
						continue; // skip if we aren't in the same device
					} else {
						if(strcmp(newDir, "/home/nkolbow/bound_dir/") == 0) {
							fprintf(stderr, "Failed mount --bind test.\n");
							fprintf(stderr, "\t${bound}.st_dev: %lu\n", getDeviceID(newDir));
							fprintf(stderr, "\t${%s}.st_dev: %lu\n", root, getDeviceID(root));
							exit(-2);
						}
					}

					if(!recursive_dir(newDir))
						collectFileList(newDir, list, device);
					else {
						char msg[1024];
						sprintf(msg, "Found cycle; skipping %s in this iteration.\n", newDir);
						d_log(DB_DEBUG, msg);
					}

					free(newDir);
				}

				if(list == NULL) {
					list = new;
				} else {
					new->next = list->next;
					list->next = new;
				}
			}
		}
	}

	closedir(curr_dir);
	return list;
}

int recursive_dir(char *dir) {
	for(int i = 0; i < GLOBAL_SRC_SIZE; i++) {
		if(strcmp(GLOBAL_SRC_LIST[i], dir) == 0) {
			return 1;
		}
	}
	return 0;
}


void freeListNode(struct dir_entry *);

void freeFileList(struct dir_entry *list) {
	if(list == NULL) {
		fprintf(stderr, "Error: freeFileList was given a NULL pointer.\n");
	} else {
		struct dir_entry *curr = list;
		while(curr != NULL) {
			list = curr->next;
			free_list_node(curr);
			curr = list;
		}
	}
}

void free_list_node(struct dir_entry *node) {
	if(node->path != NULL) free(node->path);
	if(node->link != NULL) free(node->link);
	free(node);
}

// Takes a string w/ a newline and turns that newline into a null terminator
// Used to alleviate the fgets output that ends w/ newlines
void fix_newline(char *str) {
	int i = 0;
	while(str[i] != '\0') {
		if(str[i] == '\n') {
			str[i] = '\0';
			return;
		}

		i++;
	}
}

// Generates a random integer from 0 to RND_UPPER-1 inclusive
int rnd() {
	return rand() % LOCK_LEN;
}