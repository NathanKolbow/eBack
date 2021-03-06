#include <time.h>
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
#include <sys/wait.h>

#include <mntent.h>
#include <sys/statvfs.h>

#include "back_util.h"
#include "btrfs_handler.h"
#include "io_handler.h"
#include "slave.h"

int N_SLAVES = -1;

int main(int argc, char *args[]) {
	if(argc < 3) {
		fprintf(stderr, "Usage: %s [-s #] src (dests)\n", args[0]);
		exit(-1);
	}

	int c;
	while((c = getopt(argc, args, "s:")) != -1)
		switch(c) {
			case 's':
				N_SLAVES = strtol(optarg, NULL, 10);
				if(N_SLAVES <= 0) {
					fprintf(stderr, "Error: Invalid argument for -s.\n");
					exit(-1);
				}

				break;
			case '?':
				if(optopt == 's')
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				else
					fprintf(stderr, "Unknown option '-%c'.\n", optopt);
				exit(-1);
			default:
				fprintf(stderr, "Aborting\n\toptopt: %c\n\toptarg: %s", optopt, optarg);
				abort();
		}

	char **ins = malloc(sizeof(char *) * (argc - optind));
	int i = 0;
	for(int opti = optind; opti < argc; opti++) {
		DIR *dir = opendir(args[opti]);
		if(dir) {
			if(args[opti][strlen(args[opti]) - 1] == '/') {
				ins[i] = malloc(strlen(args[opti]) + 1);
				strcpy(ins[i], args[opti]);
			} else {
				ins[i] = malloc(strlen(args[opti]) + 2);
				strcpy(ins[i], args[opti]);
				strcat(ins[i], "/");
			}
			closedir(dir);
		} else if(ENOENT == errno) {
			fprintf(stderr, "Error: %s is not a valid directory.\n", args[opti]);
			exit(-1);
		} else {
			fprintf(stderr, "Error: opendir(...) failed for an unknown reason.\n");
			exit(-1);
		}
		i++;
	}

	// if the user provided a valud for N_SLAVES, use that, otherwise use as many slaves as there are devices
	N_SLAVES = (N_SLAVES == -1) ? argc - optind - 1 : N_SLAVES;
	// first input argument is the src, all the remaining inputs are the destinations
	transfer_init(ins[0], ins+1, argc - optind - 1, N_SLAVES);

	for(int i = 0; i < argc - optind; i++) {
		free(ins[i]);
	}
	free(ins);

	// print out the timestamp
	time_t now;
	time(&now);
	char *c_now = ctime(&now);

	FILE *fp = fopen(lastback_stamp_loc, "w");
	if(fp == NULL) {
		// not a big deal, just quit
	} else {
		fwrite(c_now, strlen(c_now), 1, fp);
		fclose(fp);
	}

	return 0;
}
