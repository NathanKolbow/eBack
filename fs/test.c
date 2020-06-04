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
#include <pthread.h>

void * work(void *arg) {
	printf("Thread id: %lu\n", pthread_self());
}

int main() {
	pthread_t threads[10];
	for(int i = 0; i < 10; i++)
		pthread_create(&threads[i], NULL, work, NULL);

	sleep(5);




//	if(1) printf("YES\n");

//	int fd = open("/opt/server-setup-scripts/external-backups/eBack/fs/fff", O_CREAT | O_WRONLY, 0444);
	// printf("%d, #%d: %s\n", fd, errno, strerror(errno));

	// int ret = fchmod(fd, 0644);
	// printf("%d, #%d: %s\n", ret, errno, strerror(errno));


	struct stat info;
	stat("/opt/server-setup-scripts/external-backups/eBack/fs/fff", &info);

	printf("mode: %d\n", info.st_mode);
	printf("mode & 0777: %d\n", info.st_mode & (420));
	 chmod("/opt/server-setup-scripts/external-backups/eBack/fs/fff", info.st_mode & (420));


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
