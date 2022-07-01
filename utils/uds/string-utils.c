// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Red Hat
 */

#include "string-utils.h"

#include "errors.h"
#include "logger.h"
#include "memory-alloc.h"
#include "permassert.h"
#include "uds.h"

int uds_alloc_sprintf(const char *what, char **strp, const char *fmt, ...)
{
	va_list args;
	int result;
	if (strp == NULL) {
		return UDS_INVALID_ARGUMENT;
	}
	va_start(args, fmt);
	result = vasprintf(strp, fmt, args) == -1 ? -ENOMEM : UDS_SUCCESS;
	va_end(args);
	if ((result != UDS_SUCCESS) && (what != NULL)) {
		uds_log_error("cannot allocate %s", what);
	}
	return result;
}

int uds_wrap_vsnprintf(const char *what,
		       char *buf,
		       size_t buf_size,
		       int error,
		       const char *fmt,
		       va_list ap,
		       size_t *needed)
{
	int n;

	if (buf == NULL) {
		static char nobuf[1];

		buf = nobuf;
		buf_size = 0;
	}
	n = vsnprintf(buf, buf_size, fmt, ap);
	if (n < 0) {
		return uds_log_error_strerror(UDS_UNEXPECTED_RESULT,
					      "%s: vsnprintf failed", what);
	}
	if (needed) {
		*needed = n;
	}
	if (((size_t) n >= buf_size) && (buf != NULL) &&
	    (error != UDS_SUCCESS)) {
		return uds_log_error_strerror(error,
					      "%s: string too long", what);
	}
	return UDS_SUCCESS;
}

int uds_fixed_sprintf(const char *what,
		      char *buf,
		      size_t buf_size,
		      int error,
		      const char *fmt,
		      ...)
{
	va_list args;
	int result;

	if (buf == NULL) {
		return UDS_INVALID_ARGUMENT;
	}
	va_start(args, fmt);
	result = uds_wrap_vsnprintf(what, buf, buf_size, error, fmt, args,
				    NULL);
	va_end(args);
	return result;
}

char *uds_v_append_to_buffer(char *buffer, char *buf_end, const char *fmt,
			     va_list args)
{
	size_t n = vsnprintf(buffer, buf_end - buffer, fmt, args);

	if (n >= (size_t)(buf_end - buffer)) {
		buffer = buf_end;
	} else {
		buffer += n;
	}
	return buffer;
}

char *uds_append_to_buffer(char *buffer, char *buf_end, const char *fmt, ...)
{
	va_list ap;
	char *pos;

	va_start(ap, fmt);
	pos = uds_v_append_to_buffer(buffer, buf_end, fmt, ap);
	va_end(ap);
	return pos;
}

int uds_string_to_signed_int(const char *nptr, int *num)
{
	long value;
	int result = uds_string_to_signed_long(nptr, &value);

	if (result != UDS_SUCCESS) {
		return result;
	}
	if ((value < INT_MIN) || (value > INT_MAX)) {
		return ERANGE;
	}
	*num = (int) value;
	return UDS_SUCCESS;
}

int uds_string_to_unsigned_int(const char *nptr, unsigned int *num)
{
	unsigned long value;
	int result = uds_string_to_unsigned_long(nptr, &value);

	if (result != UDS_SUCCESS) {
		return result;
	}
	if (value > UINT_MAX) {
		return ERANGE;
	}
	*num = (unsigned int) value;
	return UDS_SUCCESS;
}

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

int uds_parse_uint64(const char *str, uint64_t *num)
{
	char *end;

	errno = 0;
	unsigned long long temp = strtoull(str, &end, 10);
	/*
	 * strtoull will always set end. On error, it could set errno to ERANGE
	 * or EINVAL.  (It also returns ULLONG_MAX when setting errno to
	 * ERANGE.)
	 */
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
