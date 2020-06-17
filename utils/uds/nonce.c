/*
 * Copyright (c) 2020 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/krusty/src/uds/nonce.c#5 $
 */

#include "nonce.h"

#include "murmur/MurmurHash3.h"
#include "numeric.h"
#include "random.h"
#include "stringUtils.h"
#include "timeUtils.h"

/*****************************************************************************/
static uint64_t hash_stuff(uint64_t start, const void *data, size_t len)
{
	uint32_t seed = start ^ (start >> 27);
	byte hash_buffer[16];
	MurmurHash3_x64_128(data, len, seed, hash_buffer);
	return get_unaligned_le64(hash_buffer + 4);
}

/*****************************************************************************/
static void *memput(void *buf, void *end, const void *data, size_t len)
{
	byte *bp = buf;
	byte *be = end;

	size_t chunk = min_size_t(len, be - bp);
	memcpy(bp, data, chunk);
	return bp + chunk;
}

/*****************************************************************************/
size_t create_unique_nonce_data(byte *buffer, size_t length)
{
	AbsTime now = currentTime(CLOCK_REALTIME);

	byte *be = buffer + length;
	byte *bp = memput(buffer, be, &now, sizeof(now));

	uint32_t rand = random_in_range(1, (1 << 30) - 1);

	bp = memput(bp, be, &rand, sizeof(rand));

	while (bp < be) {
		size_t n = min_size_t(be - bp, bp - buffer);
		memcpy(bp, buffer, n);
		bp += n;
	}

	return bp - buffer;
}

/*****************************************************************************/
uint64_t generate_master_nonce(const void *data, size_t len)
{
	return hash_stuff(0xa1b1e0fc, data, len);
}

/*****************************************************************************/
uint64_t generate_secondary_nonce(uint64_t nonce, const void *data, size_t len)
{
	return hash_stuff(nonce + 1, data, len);
}
