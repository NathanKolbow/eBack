#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mount.h> // will use this eventually
 
#include "back_util.h"
#include "btrfs_handler.h"
#include "io_handler.h" 

int main() {
	FILE *resdata_ptr = fopen(reservedata_loc, "r");
	if(resdata_ptr == NULL) {
		fprintf(stderr, "ERROR: Failed to open %s.\n", reservedata_loc);
		exit(2);
	}

	// set up the initial directory  list structure
	struct directory_data *dirList = malloc(sizeof(struct directory_data) * 10);
	for(int i = 0; i < 10; i++) {
		dirList[i].devData = malloc(sizeof(struct device_data));
	}
	int entries = 0;
	int maxSize = 10;

	char *linePtr = NULL;
	size_t n = 0;

	// read in all the reservedata and get all the different attributes from the file
	while(getline(&linePtr, &n, resdata_ptr) != -1) {
		char *dir = malloc(sizeof(char) * (strlen(linePtr) + 1));
		strcpy(dir, linePtr);
		getAfterColon(dir);

		if(getline(&linePtr, &n, resdata_ptr) == -1) {
			fprintf(stderr, "ERROR: getline() returned -1 when reading the \%\%2'd line of %s.\n", reservedata_loc);
			exit(2);
		}
		char *sizeStr = malloc(sizeof(char) * (strlen(linePtr) + 1));
		strcpy(sizeStr, linePtr);
		getAfterColon(sizeStr);

		unsigned long long size = strtol(sizeStr, NULL, 10);

		if(getline(&linePtr, &n, resdata_ptr) == -1) {
			fprintf(stderr, "ERROR: getline() returned -1 when reading the \%\%3'd line of %s.\n", reservedata_loc);
			exit(2);
		}
		char *device = malloc(sizeof(char) * (strlen(linePtr) + 1));
		strcpy(device, linePtr);
		getAfterColon(device);

		int _entry = contains(dirList, maxSize, dir);
		if(_entry == -1) {
			struct directory_data *newDir = &dirList[entries];
			newDir->dir = malloc(strlen(dir));
			strcpy(newDir->dir, dir);

			newDir->devData[0].uuid = device;
			newDir->devData[0].size = size;

			newDir->maxDevs = 10;
			newDir->totalDevs = 1;

			entries++;
			if(entries == maxSize) {
				// try to allocate space for more devices, if we can't quit
				if(realloc(dirList, sizeof(struct directory_data) + maxSize + 10) == NULL) {
					fprintf(stderr, "ERROR: Ran out of Heap memory.\n");
					exit(3);
				}
				for(int i = maxSize; i < maxSize + 10; i++) { // prep the space for the new dev_datas
					dirList[i].devData = malloc(sizeof(struct device_data));
				}

			}
		} else {
			// just add the entry to the list if the dir is already registered
			struct directory_data *dir = &dirList[_entry];
			dir->devData[dir->totalDevs].uuid = malloc((sizeof(char) * strlen(device)) + 1);
			strcpy(dir->devData[dir->totalDevs].uuid , device);
			dir->devData[dir->totalDevs].size = size;

			dir->totalDevs++;
			if(dir->totalDevs == dir->maxDevs) {
				if(realloc(dir->devData, sizeof(struct device_data) + dir->maxDevs + 10) == NULL) {
					fprintf(stderr, "ERROR: Ran out of Heap memory.\n");
					exit(3);
				}

				dir->totalDevs++;
				dir->maxDevs += 10;
			}
		}
	}

/*
 *  Implement df -h and what not with SYSTEM() and use >> in the system calls then parse the output
 */

// USE FILESYSTEM OUTPUT FROM `df` TO GUIDE WHETHER OR NOT YOU BACK UP THE STUFF IN THE DIR
// i.e., for each folder in a directory that's being backed up, run `df $dir` and if the
//       filesystem of $dir == filesystem of $backup_dir then BACK IT UP otherwise DON'T

	int i = 0;
	while(dirList[i].dir != NULL) {
		dirList[i].fileSys = getFileSystem(dirList[i].dir);
	}


	printDirData(dirList, maxSize);

	fclose(resdata_ptr);
}

// returns the index of the directory dir in the data list
// returns -1 if the dir is not currently entered
int contains(struct directory_data list[], int size, char *dir) {
	for(int i = 0; i < size; i++) {
		// if the dir for this entry is NULL, then this entry doesn't exist yet, and we're at the end of the list. quit.
		if(list[i].dir == NULL)
			break;
		if(strcmp(list[i].dir, dir) == 0)
			return i;
	}

	return -1;
}

// str should be MALLOC'd
// changes str to a new string
// e.g.:      something:other_thing --> other_thing
void getAfterColon(char *str) {
	char key[] = ":";
	int _colon = strcspn(str, key);

	int newLength = strlen(str) - _colon;
	memcpy(str, &str[_colon] + 1, newLength);
	str[newLength - 2] = '\0';
}

// For debugging, print out the dir_list struct
void printDirData(struct directory_data directoryData[], int n) {
	printf("==== DIRECTORY_DATA STRUCTURE ====\n");
	for(int i = 0; i < n; i++) {
		struct directory_data currDir = directoryData[i];
		if(currDir.dir == NULL) {
			printf("==== END OF STRUCTURE ====\n");
			return;
		} else {
			printf("\n%s\n", currDir.dir);
			for(int j = 0; j < currDir.totalDevs; j++) {
				printf("\t%s\n\t--> %llu\n\n", currDir.devData[j].uuid, currDir.devData[j].size);
			}
		}
	}
}
