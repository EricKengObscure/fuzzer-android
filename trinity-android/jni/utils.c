#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "pids.h"
#include "random.h"
#include "utils.h"

/*
 * Use this allocator if you have an object a child writes to that you want
 * all other processes to see.
 */
void * alloc_shared(unsigned int size)
{
	void *ret;

	ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if (ret == MAP_FAILED) {
		printf("mmap %u failure\n", size);
		exit(EXIT_FAILURE);
	}
	/* poison, to force users to set it to something sensible. */
	memset(ret, rand(), size);
	return ret;
}

void * __zmalloc(size_t size, const char *func)
{
	void *p;

	p = malloc(size);
	if (p == NULL) {
		/* Maybe we mlockall'd everything. Try and undo that, and retry. */
		munlockall();
		p = malloc(size);
		if (p != NULL)
			goto done;

		printf("%s: malloc(%zu) failure.\n", func, size);
		exit(EXIT_FAILURE);
	}

done:
	memset(p, 0, size);
	return p;
}

void sizeunit(unsigned long size, char *buf)
{
	/* non kilobyte aligned size? */
	if (size & 1023) {
		sprintf(buf, "%lu bytes", size);
		return;
	}

	/* < 1MB ? */
	if (size < (1024 * 1024)) {
		sprintf(buf, "%luKB", size / 1024);
		return;
	}

	/* < 1GB ? */
	if (size < (1024 * 1024 * 1024)) {
		sprintf(buf, "%ldMB", (size / 1024) / 1024);
		return;
	}

	sprintf(buf, "%ldGB", ((size / 1024) / 1024) / 1024);
}

void kill_pid(pid_t pid)
{
	int ret;
	int childno;

	childno = find_childno(pid);
	if (childno != CHILD_NOT_FOUND) {
		if (shm->children[childno]->dontkillme == TRUE)
			return;
	}

	ret = kill(pid, SIGKILL);
	if (ret != 0)
		debugf("couldn't kill pid %d [%s]\n", pid, strerror(errno));
}
