#include <stdlib.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "back_util.h"
#include "btrfs_handler.h"
#include "io_handler.h"

int BTRFS_MODE = 0; // if the filesystem is btrfs then we do NOT check sub-directories for
					// device number, subdirectories can NOT belong to another filesystem
char **BTRFS_VALID_SUBVOLUMES;
long BTRFS_VALID_SUBS_SIZE;
dev_t CACHED_BTRFS_DEV = -1;



int btrfs() {
    return BTRFS_MODE;
}

// Checks if the given path is in the list of subvolumes collected in btrfs_init(...)
// If not, return 0.  If yes, return 1 AND set the cached device to path's device number
int is_valid_sub(char *path) {  
	for(int i = 0; i < BTRFS_VALID_SUBS_SIZE; i++)
		if(strcmp(path, BTRFS_VALID_SUBVOLUMES[i]) == 0) {
			struct stat buf;
			stat(path, &buf);
			CACHED_BTRFS_DEV = buf.st_dev;
			return 1;
		}

	return 0;
}

// Collect all the initial data for the btrfs
void btrfs_init(char *root) {
	BTRFS_MODE = 1;

	// prep our command
	char cmd[64 + (strlen(root) * 2) + 1];
	sprintf(cmd, "btrfs subvolume list %s | awk '{print \"%s\"$9\"/\"}'", root, root);
	
	// first get the length of the cache that we'll need
	char count[strlen(cmd) + 8 + 1];
	sprintf(count, "%s | wc -l", cmd);
	FILE *countPtr = popen(count, "r");
	if(countPtr == NULL) {
		fprintf(stderr, "Error: Failed to run command in btrfs_init(...).\n");
		exit(-1);
	}
	char c_size[16];
	fgets(c_size, sizeof(c_size), countPtr);
	errno = 0;
	BTRFS_VALID_SUBS_SIZE = strtol(c_size, NULL, 10);
	if(errno != 0) {
		fprintf(stderr, "Error: Failed to convert command output to long in btrfs_init(...), details: %s.\n", strerror(errno));
		exit(-1);
	}
	pclose(countPtr);
	// now we can allocate the correct amount of space
	BTRFS_VALID_SUBVOLUMES = malloc(sizeof(char *) * BTRFS_VALID_SUBS_SIZE);

	// now run the command
	FILE *cmdPtr = popen(cmd, "r");
	if(countPtr == NULL) {
		fprintf(stderr, "Error: Failed to run command in btrfs_init(...).\n");
		exit(-1);
	}

	char buf[1024];
	for(int i = 0; i < BTRFS_VALID_SUBS_SIZE; i++) {
		if(fgets(buf, sizeof(buf), cmdPtr) == NULL) {
			fprintf(stderr, "Error: Loop broke earlier than expected in btrfs_init(...).\n");
			break;
		}

		fix_newline(buf); // change the new-line returned from fgets to a null terminator

		// populate the array
		BTRFS_VALID_SUBVOLUMES[i] = malloc(strlen(buf) + 1);
		strcpy(BTRFS_VALID_SUBVOLUMES[i], buf);
	}
	pclose(cmdPtr);
}

int fd_isBtrfs(int fd) {
	struct statfs buf;
	fstatfs(fd, &buf);
	return buf.f_type == 0x9123683E;
}

int isBtrfs(char *path) {
	struct statfs buf;
	statfs(path, &buf);
	return buf.f_type == 0x9123683E;
}

// Details for backing up a btrfs directory:
//   the directory being backed up MUST be the root directory of a btrfs,
//   i.e. it cannot be a subvolume nor a subdirectory inside of a btrfs
//
// If first==second they must be the same.  Otherwise, if they differ, then 
//   we check if this new subvolume is btrfs w/ parent id equal to the parent 
//   directory's btrfs id.  If so, they are the same.  Because we backup
//   directories recursively, this new dev_t will be saved for quick reference
//   and overridden once we find another subvolume (think of a 1-entry cache).
int btrfsSameDevice(dev_t first, dev_t second, char *new_path) {
	return first == second || second == CACHED_BTRFS_DEV || is_valid_sub(new_path);
}