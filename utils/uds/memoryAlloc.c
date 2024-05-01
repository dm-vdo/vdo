// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include <linux/types.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "memory-alloc.h"

enum { DEFAULT_MALLOC_ALIGNMENT = 2 * sizeof(size_t) }; // glibc malloc

/**
 * Allocate storage based on memory size and alignment, logging an error if
 * the allocation fails. The memory will be zeroed.
 *
 * @param size   The size of an object
 * @param align  The required alignment
 * @param what   What is being allocated (for error logging)
 * @param ptr    A pointer to hold the allocated memory
 *
 * @return VDO_SUCCESS or an error code
 **/
int vdo_allocate_memory(size_t size, size_t align, const char *what, void *ptr)
{
	int result;
	void *p;

	if (ptr == NULL)
		return UDS_INVALID_ARGUMENT;

	if (size == 0) {
		// We can skip the malloc call altogether.
		*((void **) ptr) = NULL;
		return VDO_SUCCESS;
	}

	if (align > DEFAULT_MALLOC_ALIGNMENT) {
		result = posix_memalign(&p, align, size);
		if (result != 0) {
			if (what != NULL)
				vdo_log_error_strerror(result,
						       "failed to posix_memalign %s (%zu bytes)",
						       what,
						       size);

			return -result;
		}
	} else {
		p = malloc(size);
		if (p == NULL) {
			result = errno;
			if (what != NULL)
				vdo_log_error_strerror(result,
						       "failed to allocate %s (%zu bytes)",
						       what,
						       size);

			return -result;
		}
	}

	memset(p, 0, size);
	*((void **) ptr) = p;
	return VDO_SUCCESS;
}

/*
 * Allocate storage based on memory size, failing immediately if the required
 * memory is not available. The memory will be zeroed.
 *
 * @param size  The size of an object.
 * @param what  What is being allocated (for error logging)
 *
 * @return pointer to the allocated memory, or NULL if the required space is
 *         not available.
 */
void *vdo_allocate_memory_nowait(size_t size, const char *what)
{
	void *p = NULL;

	vdo_allocate(size, char *, what, &p);
	return p;
}

/**********************************************************************/
void vdo_free(void *ptr)
{
	free(ptr);
}

/**
 * Reallocate dynamically allocated memory. There are no alignment guarantees
 * for the reallocated memory. If the new memory is larger than the old memory,
 * the new space will be zeroed.
 *
 * @param ptr       The memory to reallocate.
 * @param old_size  The old size of the memory
 * @param size      The new size to allocate
 * @param what      What is being allocated (for error logging)
 * @param new_ptr   A pointer to hold the reallocated pointer
 *
 * @return VDO_SUCCESS or an error code
 **/
int vdo_reallocate_memory(void *ptr,
			  size_t old_size,
			  size_t size,
			  const char *what,
			  void *new_ptr)
{
	char *new = realloc(ptr, size);

	if ((new == NULL) && (size != 0))
		return vdo_log_error_strerror(-errno,
					      "failed to reallocate %s (%zu bytes)",
					      what,
					      size);

	if (size > old_size)
		memset(new + old_size, 0, size - old_size);

	*((void **) new_ptr) = new;
	return VDO_SUCCESS;
}

/**********************************************************************/
int vdo_duplicate_string(const char *string,
			 const char *what,
			 char **new_string)
{
	int result;
	u8 *dup = NULL;

	result = vdo_allocate(strlen(string) + 1, u8, what, &dup);
	if (result != VDO_SUCCESS)
		return result;

	memcpy(dup, string, strlen(string) + 1);
	*new_string = (char *) dup;
	return VDO_SUCCESS;
}
