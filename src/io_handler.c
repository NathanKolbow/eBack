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

struct dest_lock *DEST_LOCK;
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
	SRC_PREFIX = src;

	DEST_LOCK = malloc(sizeof(struct dest_lock));
	DEST_LOCK->dest = dest;
	DEST_LOCK->len = len;
	DEST_LOCK->_curr = 0;
	DEST_LOCK->lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(DEST_LOCK->lock, NULL);

	char msg[1024];
	sprintf(msg, "Collecting file list.\n");
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

	char *dest;
	int i;
	for(i = 0; i < DEST_LOCK->len; i++) {
		dest = malloc(strlen(DEST_LOCK->dest[i]) + strlen(ent->path) + 1); // the room for SRC_PREFIX is built into ent->path
		sprintf(dest, "%s%s%s", DEST_LOCK->dest[i], SRC_PREFIX, ent->path+strlen(SRC_PREFIX));

		struct stat dest_info;

		if(ent->mtim_tv_sec == dest_info.st_mtim.tv_sec && ent->mtim_tv_sec != 0) {
			// file has an identical match already
			char msg[1024];
			sprintf(msg, "%s is the same version as %s (%lu == %lu); skipping.\n", ent->path, dest, ent->mtim_tv_sec, dest_info.st_mtim.tv_sec);
			d_log(DB_DEBUG, msg);

			free(dest);
			return;
		} else {
			// file exists but is old
			// Process for when this happens:
			//   1. Remove the old copy
			//   2. Try to create a new copy of the file on the SAME device
			//   3. If 2 fails, remove the partial copy and create a copy of the file
			//      on the CURRENT device in DEST_LOCK
			
			int r = try_backup(ent, dest, 0);
			if(r) {

			}
		}

		free(dest);
	}
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
		printf("Opened file with mode %d\n", ent->st_mode);
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
		ssize_t bytes_sent = sendfile(dest_fd, src_fd, &offset, ent->st_size);
		printf("Changing mode to %d\n", ent->st_mode);
		if(fchmod(dest_fd, ent->st_mode) == -1)
			printf("#%d: %s\n", errno, strerror(errno));

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
		int ret = mkdir_recursive(dest, 0777);
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
				
				stat(new->path, &info);
				new->stat_err = errno;
				new->atim_tv_sec = info.st_atim.tv_sec;
				new->mtim_tv_sec = info.st_mtim.tv_sec;
				new->st_mode = MODE_MASK & info.st_mode;
				new->st_size = info.st_size;


				if(entry->d_type == DT_LNK) {
					// first find where the link is pointing
					char buffer[256];
					int len = readlink(new->path, buffer, 256);
					if(len >= 256) {
						fprintf(stderr, "Error: Failed to follow the the link created by %s.\n", new->path);
					} else {
						buffer[len] = '\0';

						// resolve the path name to its absolute path
						char to_resolve[strlen(root) + strlen(buffer) + 1];
						sprintf(to_resolve, "%s%s", root, buffer);

						new->link = realpath(to_resolve, NULL);
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

					collectFileList(newDir, list, device);
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


void freeListNode(struct dir_entry *);

void freeFileList(struct dir_entry *list) {
	if(list == NULL) {
		fprintf(stderr, "Error: freeFileList was given a NULL pointer.\n");
	} else {
		struct dir_entry *curr = list;
		while(curr != NULL) {
			list = curr->next;
			freeListNode(curr);
			curr = list;
		}
	}
}

void freeListNode(struct dir_entry *node) {
	if(node->path != NULL) free(node->path);
	if(node->link != NULL) free(node->link);
	free(node);
}
