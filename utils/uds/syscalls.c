/*
 * Copyright Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA. 
 *
 * $Id: //eng/uds-releases/krusty/userLinux/uds/syscalls.c#2 $
 */

#include "syscalls.h"

#include <sys/prctl.h>
#include <unistd.h>

#include "permassert.h"

/**********************************************************************/
int logging_read(int fd,
		 void *buf,
		 size_t count,
		 const char *context,
		 ssize_t *bytes_read_ptr)
{
	int result;
	do {
		result = check_io_errors(read(fd, buf, count),
					 __func__,
					 context,
					 bytes_read_ptr);
	} while (result == EINTR);
	return result;
}

/**********************************************************************/
static int logging_pread_interruptible(int fd,
				       void *buf,
				       size_t count,
				       off_t offset,
				       const char *context,
				       ssize_t *bytes_read_ptr)
{
	return check_io_errors(pread(fd, buf, count, offset),
			       __func__,
			       context,
			       bytes_read_ptr);
}

/**********************************************************************/
int logging_pread(int fd,
		  void *buf,
		  size_t count,
		  off_t offset,
		  const char *context,
		  ssize_t *bytes_read_ptr)
{
	int result;
	do {
		result = logging_pread_interruptible(fd, buf, count, offset,
						     context, bytes_read_ptr);
	} while (result == EINTR);

	return result;
}

/**********************************************************************/
int logging_write(int fd,
		  const void *buf,
		  size_t count,
		  const char *context,
		  ssize_t *bytes_written_ptr)
{
	int result;
	do {
		result = check_io_errors(write(fd, buf, count),
					 __func__,
					 context,
					 bytes_written_ptr);
	} while (result == EINTR);

	return result;
}

/**********************************************************************/
static int logging_pwrite_interruptible(int fd,
					const void *buf,
					size_t count,
					off_t offset,
					const char *context,
					ssize_t *bytes_written_ptr)
{
	return check_io_errors(pwrite(fd, buf, count, offset),
			       __func__,
			       context,
			       bytes_written_ptr);
}

/**********************************************************************/
int logging_pwrite(int fd,
		   const void *buf,
		   size_t count,
		   off_t offset,
		   const char *context,
		   ssize_t *bytes_written_ptr)
{
	int result;
	do {
		result = logging_pwrite_interruptible(fd, buf, count, offset,
						      context,
						      bytes_written_ptr);
	} while (result == EINTR);

	return result;
}

/**********************************************************************/
int logging_close(int fd, const char *context)
{
	return check_system_call(close(fd), __func__, context);
}

/**********************************************************************/
int process_control(int option,
		    unsigned long arg2,
		    unsigned long arg3,
		    unsigned long arg4,
		    unsigned long arg5)
{
	int result = prctl(option, arg2, arg3, arg4, arg5);
	return ASSERT_WITH_ERROR_CODE(result >= 0,
				      errno,
				      "option: %d, arg2: %lu, arg3: %lu, arg4: %lu, arg5: %lu",
				      option,
				      arg2,
				      arg3,
				      arg4,
				      arg5);
}
