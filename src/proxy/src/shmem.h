#ifndef SHMEM_H
#define SHMEM_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Maps shared memory
 * If successfull, writes the mapped shmem pointer into @shmem
 * @return 0 for success
 */
int init_shared_memory(char **shmem);

ssize_t ivshmem_write(void *buf, size_t count, off_t offset);

int get_write_doorbell(void);

void reset_write_doorbell(void);

#endif // SHMEM_H
