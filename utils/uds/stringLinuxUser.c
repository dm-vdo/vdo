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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "logger.h"
#include "stringUtils.h"
#include "uds.h"

/**********************************************************************/
int uds_string_to_signed_long(const char *nptr, long *num)
{
	if (nptr == NULL || *nptr == '\0') {
		return UDS_INVALID_ARGUMENT;
	}
	errno = 0;
	char *endptr;
	*num = strtol(nptr, &endptr, 10);
	if (*endptr != '\0') {
		return UDS_INVALID_ARGUMENT;
	}
	return errno;
}

/**********************************************************************/
int uds_string_to_unsigned_long(const char *nptr, unsigned long *num)
{
	if (nptr == NULL || *nptr == '\0') {
		return UDS_INVALID_ARGUMENT;
	}
	errno = 0;
	char *endptr;
	*num = strtoul(nptr, &endptr, 10);
	if (*endptr != '\0') {
		return UDS_INVALID_ARGUMENT;
	}
	return errno;
}

/**********************************************************************/
char *uds_next_token(char *str, const char *delims, char **state)
{
	return strtok_r(str, delims, state);
}

/**********************************************************************/
int uds_parse_uint64(const char *str, uint64_t *num)
{
	char *end;
	errno = 0;
	unsigned long long temp = strtoull(str, &end, 10);
	// strtoull will always set end. On error, it could set errno to ERANGE
	// or EINVAL.  (It also returns ULLONG_MAX when setting errno to
	// ERANGE.)
	if ((errno == ERANGE) || (errno == EINVAL) || (*end != '\0')) {
		return UDS_INVALID_ARGUMENT;
	}
	uint64_t n = temp;
	if (temp != (unsigned long long) n) {
		return UDS_INVALID_ARGUMENT;
	}

	*num = n;
	return UDS_SUCCESS;
}
