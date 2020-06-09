all: cback.c io_handler.c btrfs_handler.c slave.c debug.c include/back_util.h include/btrfs_handler.h include/io_handler.h include/slave.h include/debug.h
	gcc -Wall -pthread -lpthread -D_REENTRANT -g -I'./include' -Wall -o cback -std=gnu99 cback.c io_handler.c btrfs_handler.c slave.c debug.c
