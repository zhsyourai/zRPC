//
// Created by zhsyourai on 12/6/16.
//

#ifndef ZRPC_FD_H
#define ZRPC_FD_H


#include <sys/types.h>
#include "zRPC/support/runnable.h"

struct zRPC_context;

typedef struct zRPC_sample_fd zRPC_sample_fd;

/* THIS function work for zRPC_sample_fd */

zRPC_sample_fd *zRPC_fd_create(int fd);

void zRPC_fd_destroy(zRPC_sample_fd *fd);

int zRPC_fd_origin(zRPC_sample_fd *fd);

void zRPC_fd_close(zRPC_sample_fd *fd);

ssize_t zRPC_fd_read(zRPC_sample_fd *fd, void *buf, size_t len);

ssize_t zRPC_fd_write(zRPC_sample_fd *fd, void *buf, size_t len);

#endif //ZRPC_FD_H
