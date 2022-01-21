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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/syscalls.h#9 $
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#include <errno.h>

#include "compiler.h"
#include "errors.h"
#include "logger.h"
#include "typeDefs.h"

/**
 * Wrap the read(2) system call, looping as long as errno is EINTR.
 *
 * @param fd             The descriptor from which to read
 * @param buf            The buffer to read into
 * @param count          The maximum number of bytes to read
 * @param context        The calling context (for logging)
 * @param bytes_read_ptr A pointer to hold the number of bytes read
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check logging_read(int fd,
			      void *buf,
			      size_t count,
			      const char *context,
			      ssize_t *bytes_read_ptr);

/**
 * Wrap the pread(2) system call, looping as long as errno is EINTR.
 *
 * @param fd             The descriptor from which to read
 * @param buf            The buffer to read into
 * @param count          The maximum number of bytes to read
 * @param offset         The offset into the file at which to read
 * @param context        The calling context (for logging)
 * @param bytes_read_ptr A pointer to hold the number of bytes read
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check logging_pread(int fd,
			       void *buf,
			       size_t count,
			       off_t offset,
			       const char *context,
			       ssize_t *bytes_read_ptr);

/**
 * Wrap the write(2) system call, looping as long as errno is EINTR.
 *
 * @param fd                The descriptor from which to write
 * @param buf               The buffer to write from
 * @param count             The maximum number of bytes to write
 * @param context           The calling context (for logging)
 * @param bytes_written_ptr A pointer to hold the number of bytes written;
 *                          on error, -1 is returned
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check logging_write(int fd,
			       const void *buf,
			       size_t count,
			       const char *context,
			       ssize_t *bytes_written_ptr);

/**
 * Wrap the pwrite(2) system call, looping as long as errno is EINTR.
 *
 * @param fd                The descriptor from which to write
 * @param buf               The buffer to write into
 * @param count             The maximum number of bytes to write
 * @param offset            The offset into the file at which to write
 * @param context           The calling context (for logging)
 * @param bytes_written_ptr A pointer to hold the number of bytes written;
 *                          on error, -1 is returned
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check logging_pwrite(int fd,
				const void *buf,
				size_t count,
				off_t offset,
				const char *context,
				ssize_t *bytes_written_ptr);

/**
 * Wrap the close(2) system call.
 *
 * @param fd      The descriptor to close
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check logging_close(int fd, const char *context);

/**
 * Perform operations on a process.
 * This wraps the prctl(2) function, q.v.
 *
 * @param option     The operation to perform.
 * @param arg2       Specific to option
 * @param arg3       Specific to option
 * @param arg4       Specific to option
 * @param arg5       Specific to option
 *
 * @return UDS_SUCCESS or an error code
 **/
int process_control(int option,
		    unsigned long arg2,
		    unsigned long arg3,
		    unsigned long arg4,
		    unsigned long arg5);

/**********************************************************************/
static INLINE int log_system_call_errno(const char *function,
					const char *context)
{
	return uds_log_strerror(((errno == EINTR) ? UDS_LOG_DEBUG
						  : UDS_LOG_ERR),
				errno,
				"%s failed in %s",
				function,
				context);
}

/**********************************************************************/
static INLINE int
check_system_call(int result, const char *function, const char *context)
{
	return ((result == 0) ? UDS_SUCCESS :
				log_system_call_errno(function, context));
}

/**********************************************************************/
static INLINE int check_io_errors(ssize_t bytes,
				  const char *function,
				  const char *context,
				  ssize_t *bytes_ptr)
{
	if (bytes_ptr != NULL) {
		*bytes_ptr = bytes;
	}
	if (bytes < 0) {
		return log_system_call_errno(function, context);
	}
	return UDS_SUCCESS;
}

#endif /* SYSCALLS_H */
