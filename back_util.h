#ifndef _BACK_UTIL_H_
#define _BACK_UTIL_H_

// Constants
const char *housing_dir = "/opt/server-setup-scripts/external-backups";
const char *timestamp_loc = "/opt/server-setup-scripts/external-backups/lastdate";
const char *devdata_loc = "/opt/server-setup-scripts/external-backups/devices";
const char *reservedata_loc = "/opt/server-setup-scripts/external-backups/reserved";
const char *lastback_stamp_loc = "/opt/server-setup-scripts/external-backups/.lastback";

// Structures
struct device_data {
	char *uuid;
	unsigned long long size;
};

struct directory_data {
        char *dir;
	char *fileSys; // the filesystem holding the directory, used to make sure that
		       // backups don't cross filesystems
        struct device_data *devData;
        int totalDevs;
	int maxDevs;
};

// Functions
void getAfterColon(char *);
int contains(struct directory_data[], int, char *);
void printDirData(struct directory_data[], int);
char * getFileSystem(char *);

#endif  // _BACK_UTIL_H
