#ifndef _BACK_UTIL_H_
#define _BACK_UTIL_H_

// Constants
#define housing_dir "/opt/server-setup-scripts/external-backups"
#define timestamp_loc "/opt/server-setup-scripts/external-backups/lastdate"
#define devdata_loc "/opt/server-setup-scripts/external-backups/devices"
#define reservedata_loc "/opt/server-setup-scripts/external-backups/reserved"
#define lastback_stamp_loc "/opt/server-setup-scripts/external-backups/.lastback"

#define DIR_ENTRY_PREALLOC_INC 1000

// Structures
struct device_data {
	char *uuid;
	unsigned long long size;
};

struct directory_data {
    char *dir;
	char *fileSys;	// the filesystem holding the directory, used to make sure that
		       		// backups don't cross filesystems
    struct device_data *devData;
    int totalDevs;
	int maxDevs;
};

// Functions
void getAfterColon(char *);
int contains(struct directory_data[], int, char *);
void printDirData(struct directory_data[], int);


#endif  // _BACK_UTIL_H
