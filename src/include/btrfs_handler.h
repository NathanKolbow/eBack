#ifndef _BTRFS_HANDLER_H_
#define _BTRFS_HANDLER_H_

#include <sys/types.h>

int btrfs();
void btrfs_init(char *);
int fd_isBtrfs();
int isBtrfs(char *);
int btrfsSameDevice(dev_t, dev_t, char *);

#endif // _BTRFS_HANDLER_H_