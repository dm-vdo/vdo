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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/fileUtils.c#12 $
 */

#include "fileUtils.h"

#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "errors.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "numeric.h"
#include "permassert.h"
#include "stringUtils.h"
#include "syscalls.h"

/**********************************************************************/
int file_exists(const char *path, bool *exists)
{
	struct stat stat_buf;
	int result = logging_stat_missing_ok(path, &stat_buf, __func__);

	if (result == UDS_SUCCESS) {
		*exists = true;
	} else if (result == ENOENT) {
		*exists = false;
		result = UDS_SUCCESS;
	}

	return result;
}

/**********************************************************************/
int open_file(const char *path, enum file_access access, int *fd)
{
	int ret_fd;
	int flags;
	mode_t mode;

	switch (access) {
	case FU_READ_ONLY:
		flags = O_RDONLY;
		mode = 0;
		break;
	case FU_READ_WRITE:
		flags = O_RDWR;
		mode = 0;
		break;
	case FU_CREATE_READ_WRITE:
		flags = O_CREAT | O_RDWR | O_TRUNC;
		mode = 0666;
		break;
	case FU_CREATE_WRITE_ONLY:
		flags = O_CREAT | O_WRONLY | O_TRUNC;
		mode = 0666;
		break;
	case FU_READ_ONLY_DIRECT:
		flags = O_RDONLY | O_DIRECT;
		mode = 0;
		break;
	case FU_READ_WRITE_DIRECT:
		flags = O_RDWR | O_DIRECT;
		mode = 0;
		break;
	case FU_CREATE_READ_WRITE_DIRECT:
		flags = O_CREAT | O_RDWR | O_TRUNC | O_DIRECT;
		mode = 0666;
		break;
	case FU_CREATE_WRITE_ONLY_DIRECT:
		flags = O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT;
		mode = 0666;
		break;
	default:
		return uds_log_warning_strerror(UDS_INVALID_ARGUMENT,
						"invalid access mode opening file %s",
						path);
	}

	do {
		ret_fd = open(path, flags, mode);
	} while ((ret_fd == -1) && (errno == EINTR));
	if (ret_fd < 0) {
		return uds_log_error_strerror(errno,
					      "open_file(): failed opening %s with file access: %d",
					      path,
					      access);
	}
	*fd = ret_fd;
	return UDS_SUCCESS;
}

/**********************************************************************/
int close_file(int fd, const char *error_message)
{
	return logging_close(fd, error_message);
}

/**********************************************************************/
void try_close_file(int fd)
{
	int old_errno = errno;
	int result = close_file(fd, __func__);
	errno = old_errno;
	if (result != UDS_SUCCESS) {
		uds_log_debug_strerror(result, "error closing file");
	}
}

/**********************************************************************/
int sync_and_close_file(int fd, const char *error_message)
{
	int result = logging_fsync(fd, error_message);
	if (result != UDS_SUCCESS) {
		try_close_file(fd);
		return result;
	}
	return close_file(fd, error_message);
}

/**********************************************************************/
void try_sync_and_close_file(int fd)
{
	int result = sync_and_close_file(fd, __func__);
	if (result != UDS_SUCCESS) {
		uds_log_debug_strerror(result,
				       "error syncing and closing file");
	}
}

/**********************************************************************/
int read_buffer(int fd, void *buffer, unsigned int length)
{
	byte *ptr = buffer;
	size_t bytes_to_read = length;

	while (bytes_to_read > 0) {
		ssize_t bytes_read;
		int result = logging_read(fd, ptr, bytes_to_read, __func__,
					  &bytes_read);
		if (result != UDS_SUCCESS) {
			return result;
		}

		if (bytes_read == 0) {
			return uds_log_warning_strerror(UDS_CORRUPT_FILE,
							"unexpected end of file while reading");
		}

		ptr += bytes_read;
		bytes_to_read -= bytes_read;
	}

	return UDS_SUCCESS;
}

/**********************************************************************/
int read_data_at_offset(int fd,
			off_t offset,
			void *buffer,
			size_t size,
			size_t *length)
{
	byte *ptr = buffer;
	size_t bytes_to_read = size;
	off_t current_offset = offset;

	while (bytes_to_read > 0) {
		ssize_t bytes_read;
		int result = logging_pread(fd,
					   ptr,
					   bytes_to_read,
					   current_offset,
					   __func__,
					   &bytes_read);
		if (result != UDS_SUCCESS) {
			return result;
		}

		if (bytes_read == 0) {
			break;
		}
		ptr += bytes_read;
		bytes_to_read -= bytes_read;
		current_offset += bytes_read;
	}

	*length = ptr - (byte *) buffer;
	return UDS_SUCCESS;
}


/**********************************************************************/
int write_buffer(int fd, const void *buffer, unsigned int length)
{
	size_t bytes_to_write = length;
	const byte *ptr = buffer;
	while (bytes_to_write > 0) {
		ssize_t written;
		int result = logging_write(fd, ptr, bytes_to_write, __func__,
					   &written);
		if (result != UDS_SUCCESS) {
			return result;
		}

		if (written == 0) {
			// this should not happen, but if it does, errno won't
			// be defined, so we need to return our own error
			return uds_log_error_strerror(UDS_UNKNOWN_ERROR,
						      "wrote 0 bytes");
		}
		bytes_to_write -= written;
		ptr += written;
	}
	return UDS_SUCCESS;
}

/**********************************************************************/
int write_buffer_at_offset(int fd,
			   off_t offset,
			   const void *buffer,
			   size_t length)
{
	size_t bytes_to_write = length;
	const byte *ptr = buffer;
	off_t current_offset = offset;

	while (bytes_to_write > 0) {
		ssize_t written;
		int result = logging_pwrite(fd,
					    ptr,
					    bytes_to_write,
					    current_offset,
					    __func__,
					    &written);
		if (result != UDS_SUCCESS) {
			return result;
		}

		if (written == 0) {
			// this should not happen, but if it does, errno won't
			// be defined, so we need to return our own error
			return uds_log_error_strerror(UDS_UNKNOWN_ERROR,
						      "impossible write error");
		}

		bytes_to_write -= written;
		ptr += written;
		current_offset += written;
	}

	return UDS_SUCCESS;
}

/**********************************************************************/
int get_open_file_size(int fd, off_t *size_ptr)
{
	struct stat statbuf;

	if (logging_fstat(fd, &statbuf, "get_open_file_size()") == -1) {
		return errno;
	}
	*size_ptr = statbuf.st_size;
	return UDS_SUCCESS;
}

/**********************************************************************/
int remove_file(const char *file_name)
{
	int result = unlink(file_name);
	if (result == 0 || errno == ENOENT) {
		return UDS_SUCCESS;
	}
	return uds_log_warning_strerror(errno, "Failed to remove %s",
					file_name);
}

/**********************************************************************/
bool file_name_match(const char *pattern, const char *string, int flags)
{
	int result = fnmatch(pattern, string, flags);
	if ((result != 0) && (result != FNM_NOMATCH)) {
		uds_log_error("file_name_match(): fnmatch(): returned an error: %d, looking for \"%s\" with flags: %d",
			      result,
			      string,
			      flags);
	}
	return (result == 0);
}

/**********************************************************************/
int make_abs_path(const char *path, char **abs_path)
{
	char *tmp;
	int result = UDS_SUCCESS;
	if (path[0] == '/') {
		result = uds_duplicate_string(path, __func__, &tmp);
	} else {
		char *cwd = get_current_dir_name();
		if (cwd == NULL) {
			return errno;
		}
		result = alloc_sprintf(__func__, &tmp, "%s/%s", cwd, path);
		UDS_FREE(cwd);
	}
	if (result == UDS_SUCCESS) {
	*abs_path = tmp;
	}
	return result;
}

/**********************************************************************/
int logging_stat(const char *path, struct stat *buf, const char *context)
{
	if (stat(path, buf) == 0) {
		return UDS_SUCCESS;
	}
	return uds_log_error_strerror(errno, "%s failed in %s for path %s",
				      __func__, context, path);
}

/**********************************************************************/
int logging_stat_missing_ok(const char *path,
			    struct stat *buf,
			    const char *context)
{
	if (stat(path, buf) == 0) {
		return UDS_SUCCESS;
	}
	if (errno == ENOENT) {
		return errno;
	}
	return uds_log_error_strerror(errno, "%s failed in %s for path %s",
				      __func__, context, path);
}

/**********************************************************************/
int logging_fstat(int fd, struct stat *buf, const char *context)
{
	return check_system_call(fstat(fd, buf), __func__, context);
}

/**********************************************************************/
int logging_fsync(int fd, const char *context)
{
	return check_system_call(fsync(fd), __func__, context);
}
