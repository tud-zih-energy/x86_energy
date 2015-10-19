/*
 libmsr.so, libmsr_static.a
 a library to limit the open file handles to msr registers by using reference counting
 Copyright (C) 2012 TU Dresden, ZIH

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, v2, as
 published by the Free Software Foundation

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "msr.h"

#ifndef MAX_NUM_THREADS
#define MAX_NUM_THREADS 128
#endif

static int files[MAX_NUM_THREADS];
static unsigned int references[MAX_NUM_THREADS];
static unsigned int oflag_busy_locks[MAX_NUM_THREADS];
static pthread_mutex_t muteces[MAX_NUM_THREADS];
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static int isInit = 0;
static int oflag = O_RDONLY;

static int change_oflag(int cpu);
/**
 * Checks if the msr module is loaded and accessible by the user
 * The reference array is intialized to 0 and their correponding mutex is intialized
 */
int init_msr(int init_oflag)
{
    int i;
    int fd;
    const char* msr_file_name = "/dev/cpu/0/msr";

    /* prevents multiple intializations and race condition */
    pthread_mutex_lock(&global_lock);
    if (!isInit) {
        memset(references,0,MAX_NUM_THREADS*sizeof(unsigned int));
        memset(oflag_busy_locks,0,MAX_NUM_THREADS*sizeof(unsigned int));
        for(i=0;i<MAX_NUM_THREADS;i++) {
            pthread_mutex_init(&muteces[i], NULL);
        }
        isInit = 1;
    }

    /* open all files */
    fd = open(msr_file_name, init_oflag);

    if (fd < 0) {
        fprintf(stderr, "ERROR\n");
        fprintf(stderr, "rdmsr: failed to open '%s': %s!\n",msr_file_name , strerror(errno));
        fprintf(stderr, "       Please check if the msr module is loaded and the device file has correct permissions.\n\n");
        pthread_mutex_unlock(&global_lock);
        return fd;
    }

    close(fd);

    if (oflag == O_RDONLY && init_oflag == O_RDWR) {
        oflag = init_oflag;
        for (i=0; i<MAX_NUM_THREADS; i++) {
            int ret = change_oflag(i);
            if (ret) {
                pthread_mutex_unlock(&global_lock);
                return ret;
            }
        }
    }

    pthread_mutex_unlock(&global_lock);
    return 0;
}

/**
 * reopens the file descriptor from the corresponding cpu
 * and changes the oflag from O_RDONLY to O_RDWR
 */
static int change_oflag(int cpu)
{
    pthread_mutex_lock(&muteces[cpu]);
    oflag_busy_locks[cpu] = 1;
    if (references[cpu] != 0) {
        char* msr_file_name = (char*) alloca(20 * sizeof(char));
        sprintf(msr_file_name,"/dev/cpu/%d/msr",cpu);
        close(files[cpu]);
        files[cpu] = open(msr_file_name, oflag);

        if (files[cpu] < 0) {
            pthread_mutex_unlock(&muteces[cpu]);
            return files[cpu];
        }
    }
    oflag_busy_locks[cpu] = 0;
    pthread_mutex_unlock(&muteces[cpu]);
    return 0;
}

/**
 * Checks if there is an open file descriptor for the given cpu
 * otherwise it will open the file descriptor.
 * Saves the cpu and msr register in the give handle.
 */
int open_msr(uint32_t cpu, uint32_t msr, struct msr_handle * handle)
{
    pthread_mutex_lock(&muteces[cpu]);
    if (references[cpu] == 0) {
        char* msr_file_name = (char*) alloca(20 * sizeof(char));
        sprintf(msr_file_name,"/dev/cpu/%d/msr",cpu);
        files[cpu] = open(msr_file_name, oflag);

        if (files[cpu] < 0) {
            pthread_mutex_unlock(&muteces[cpu]);
            return files[cpu];
        }
    }
    handle->cpu=cpu;
    handle->msr=msr;
    references[cpu]++;
    pthread_mutex_unlock(&muteces[cpu]);
    return 0;
}

/**
 * Reads the msr register on the cpu given by the handle.
 * The result is saved in the data variable of the handle.
 */
int read_msr(struct msr_handle * handle)
{
    uint32_t cpu = handle->cpu;
    if (pread(files[cpu], &handle->data, sizeof(uint64_t), handle->msr) != sizeof(uint64_t)) {
        if (errno == EBADF && references[cpu]) {
            while (oflag_busy_locks[cpu])
                ; /* busy waiting */
            return read_msr(handle);
        }
        else {
            fprintf(stderr,"Error reading cpu %d reg %x\n",cpu, handle->msr);
            return errno;
        }
    }
    return 0;
}

/**
 * Writes the handle.data value to the msr register on the cpu given by the handle.
 */
int write_msr(struct msr_handle handle)
{
    uint32_t cpu = handle.cpu;
    if (pwrite(files[cpu], &handle.data, sizeof(uint64_t),  handle.msr) != sizeof(uint64_t)) {
        if (errno == EBADF && oflag == O_RDWR && references[cpu]) {
            while (oflag_busy_locks[cpu])
                ; /* busy waiting */
            return write_msr(handle);
        }
        else {
            fprintf(stderr,"Error writing cpu %d reg %x\n",cpu, handle.msr);
            return errno;
        }
    }
    return 0;
}

/**
 * Decreases number of references to the cpu filedescriptor.
 * If the refrence counter reaches zero the filedescriptor will be closed.
 */
void close_msr(struct msr_handle handle)
{
    pthread_mutex_lock(&muteces[handle.cpu]);
    references[handle.cpu]--;
    if (references[handle.cpu] == 0) {
        close(files[handle.cpu]);
    }
    pthread_mutex_unlock(&muteces[handle.cpu]);
}
