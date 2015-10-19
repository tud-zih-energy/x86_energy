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

#ifndef MSR_H_
#define MSR_H_

#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>

struct msr_handle{
  uint32_t cpu;
  uint32_t msr;
  uint64_t data;
};

/**
 * Checks if the msr module is loaded and accessible by the user
 * The reference array is intialized to 0 and their correponding mutex is intialized
 */
int init_msr(int init_oflag);

/**
 * Checks if there is an open file descriptor for the given cpu
 * otherwise it will open the file descriptor.
 * Saves the cpu and msr register in the give handle.
 */
int open_msr(uint32_t cpu, uint32_t msr, struct msr_handle * handle);

/**
 * Reads the msr register on the cpu given by the handle.
 * The result is saved in the data variable of the handle.
 */
int read_msr(struct msr_handle * handle);
/**
 * Writes the handle.data value to the msr register on the cpu given by the handle.
 */
int write_msr(struct msr_handle handle);

/**
 * Decreases number of references to the cpu filedescriptor.
 * If the refrence counter reaches zero the filedescriptor will be closed.
 */
void close_msr(struct msr_handle handle);


#endif /* MSR_H_ */
