#include <fcntl.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

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
			if(mkdir(tmp, mode) == -1 && errno != EEXIST) { printf("%s, %d: %s\n", tmp, errno, strerror(errno)); return 0; }
			*p = '/';
		}
	if(mkdir(tmp, mode) == -1) { printf("%s, %d: %s\n", tmp, errno, strerror(errno)); return 0; }

	return 1;
}

int main() {
//	if(1) printf("YES\n");


	mkdir_recursive("/opt/server-setup-scripts/external-backups/fs/folder1/folder2/folder3/folder4/", 0777);


//	char *SRC_PREFIX = "/opt/PepPrograms/";
//	char *src = "/opt/PepPrograms/anaconda3/bin/python3";
//	char *dest = "/home/nkolbow/";

//	char out[1024];
//	sprintf(out, "%s%s", dest, src + strlen(SRC_PREFIX));
//	printf("%s\n", out);



//	struct stat info;
//	int r = stat("/fake/news/", &info);

//	printf("ENOENT: %d\nr: %d\nerrno: %d\nstrerror: %s\n", ENOENT, r, errno, strerror(errno));



//	char *in_file = "/opt/server-setup-scripts/external-backups/fs/file";
//	char *out_file = "/opt/server-setup-scripts/external-backups/fs/mount/backup.nat";

//	int in_fd = open(in_file, O_RDONLY);
//	int out_fd = open(out_file, O_CREAT | O_WRONLY);

//	struct stat src_info;
//	stat(in_file, &src_info);

//	off_t offset = 0;
//	ssize_t ret = sendfile(out_fd, in_fd, &offset, src_info.st_size);

//	printf("Error #%d: %s\n", errno, strerror(errno));
//	printf("ret: %lu; expected: %lu\n", ret, src_info.st_size);
}
