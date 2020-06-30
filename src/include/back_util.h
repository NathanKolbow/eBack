#ifndef _BACK_UTIL_H_
#define _BACK_UTIL_H_

// Constants
#define housing_dir "/etc/eback"
#define timestamp_loc "/etc/eback/lastdate"
#define devdata_loc "/etc/eback/devices"
#define reservedata_loc "/etc/eback/reserved"
#define lastback_stamp_loc "/etc/eback/.lastback"
#define dirlist_loc "/etc/eback/dirs"

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
