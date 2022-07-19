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
 */

#include <errno.h>

#include "logger.h"
#include "memory-alloc.h"
#include "string-utils.h"

/**********************************************************************/
int uds_allocate_memory(size_t size, size_t align, const char *what, void *ptr)
{
	if (ptr == NULL) {
		return UDS_INVALID_ARGUMENT;
	}
	if (size == 0) {
		// We can skip the malloc call altogether.
		*((void **) ptr) = NULL;
		return UDS_SUCCESS;
	}
	void *p;
	enum { DEFAULT_MALLOC_ALIGNMENT = 2 * sizeof(size_t) }; // glibc malloc
	if (align > DEFAULT_MALLOC_ALIGNMENT) {
		int result = posix_memalign(&p, align, size);
		if (result != 0) {
			if (what != NULL) {
				uds_log_error_strerror(result,
						       "failed to posix_memalign %s (%zu bytes)",
						       what,
						       size);
			}
			return -result;
		}
	} else {
		p = malloc(size);
		if (p == NULL) {
			int result = errno;
			if (what != NULL) {
				uds_log_error_strerror(result,
						       "failed to allocate %s (%zu bytes)",
						       what,
						       size);
			}
			return -result;
		}
	}
	memset(p, 0, size);
	*((void **) ptr) = p;
	return UDS_SUCCESS;
}

/**********************************************************************/
void *uds_allocate_memory_nowait(size_t size, const char *what) {
	void *p = NULL;

	UDS_ALLOCATE(size, char *, what, &p);
	return p;
}

/**********************************************************************/
void uds_free_memory(void *ptr)
{
	free(ptr);
}

/**********************************************************************/
int uds_reallocate_memory(void *ptr,
			  size_t old_size,
			  size_t size,
			  const char *what,
			  void *new_ptr)
{
	char *new = realloc(ptr, size);
	if ((new == NULL) && (size != 0)) {
		return uds_log_error_strerror(-errno,
					      "failed to reallocate %s (%zu bytes)",
					      what,
					      size);
	}

	if (size > old_size) {
		memset(new + old_size, 0, size - old_size);
	}

	*((void **) new_ptr) = new;
	return UDS_SUCCESS;
}

/**********************************************************************/
int uds_duplicate_string(const char *string,
			 const char *what,
			 char **new_string)
{
	byte *dup = NULL;
	int result = UDS_ALLOCATE(strlen(string) + 1, byte, what, &dup);
	if (result != UDS_SUCCESS) {
		return result;
	}

	memcpy(dup, string, strlen(string) + 1);
	*new_string = (char *) dup;
	return UDS_SUCCESS;
}
