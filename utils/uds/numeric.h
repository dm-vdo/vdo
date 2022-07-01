/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright Red Hat
 */

#ifndef NUMERIC_H
#define NUMERIC_H 1

#include "compiler.h"

#include "numericDefs.h"
#include "type-defs.h"

/*
 * Type safe comparison macros, similar to the ones in linux/kernel.h.
 */
/* If pointers to types are comparable (without dereferencing them and
 * potentially causing side effects) then types are the same.
 */
#define TYPECHECK(x, y) (!!(sizeof((typeof(x) *) 1 == (typeof(y) *) 1)))
#define CONSTCHECK(x, y) (__builtin_constant_p(x) && __builtin_constant_p(y))

/* It takes two levels of macro expansion to compose the unique temp names. */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNIQUE_ID(a) CONCAT(_UNIQUE_, CONCAT(a, __COUNTER__))

#define SAFE_COMPARE(x, y, unique_x, unique_y, op)          \
	__extension__({                                     \
		typeof(x) unique_x = (x);                   \
		typeof(y) unique_y = (y);                   \
		unique_x op unique_y ? unique_x : unique_y; \
	})

#define COMPARE(x, y, op)                              \
	__builtin_choose_expr(                         \
		(TYPECHECK(x, y) && CONSTCHECK(x, y)), \
		(((x) op(y)) ? (x) : (y)),             \
		SAFE_COMPARE(x, y, UNIQUE_ID(x_), UNIQUE_ID(y_), op))

#define min(x, y) COMPARE(x, y, <)
#define max(x, y) COMPARE(x, y, >)

// Copied from linux/math.h
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/**
 * Extract a 64 bit signed little-endian number from a buffer at a
 * specified offset.  The offset will be advanced to the first byte
 * after the number.
 *
 * @param buffer  The buffer from which to extract the number
 * @param offset  A pointer to the offset into the buffer at which to extract
 * @param decoded A pointer to hold the extracted number
 **/
static INLINE void decode_int64_le(const uint8_t *buffer,
				   size_t *offset,
				   int64_t *decoded)
{
	*decoded = get_unaligned_le64(buffer + *offset);
	*offset += sizeof(int64_t);
}

/**
 * Encode a 64 bit signed number into a buffer at a given offset using
 * a little-endian representation. The offset will be advanced to
 * first byte after the encoded number.
 *
 * @param data      The buffer to encode into
 * @param offset    A pointer to the offset at which to start encoding
 * @param to_encode The number to encode
 **/
static INLINE void encode_int64_le(uint8_t *data,
				   size_t *offset,
				   int64_t to_encode)
{
	put_unaligned_le64(to_encode, data + *offset);
	*offset += sizeof(int64_t);
}

/**
 * Extract a 64 bit unsigned little-endian number from a buffer at a
 * specified offset.  The offset will be advanced to the first byte
 * after the number.
 *
 * @param buffer  The buffer from which to extract the number
 * @param offset  A pointer to the offset into the buffer at which to extract
 * @param decoded A pointer to hold the extracted number
 **/
static INLINE void decode_uint64_le(const uint8_t *buffer,
				    size_t *offset,
				    uint64_t *decoded)
{
	*decoded = get_unaligned_le64(buffer + *offset);
	*offset += sizeof(uint64_t);
}

/**
 * Encode a 64 bit unsigned number into a buffer at a given offset
 * using a little-endian representation. The offset will be advanced
 * to first byte after the encoded number.
 *
 * @param data      The buffer to encode into
 * @param offset    A pointer to the offset at which to start encoding
 * @param to_encode The number to encode
 **/
static INLINE void encode_uint64_le(uint8_t *data,
				    size_t *offset,
				    uint64_t to_encode)
{
	put_unaligned_le64(to_encode, data + *offset);
	*offset += sizeof(uint64_t);
}

/**
 * Extract a 32 bit signed little-endian number from a buffer at a
 * specified offset.  The offset will be advanced to the first byte
 * after the number.
 *
 * @param buffer  The buffer from which to extract the number
 * @param offset  A pointer to the offset into the buffer at which to extract
 * @param decoded A pointer to hold the extracted number
 **/
static INLINE void decode_int32_le(const uint8_t *buffer,
				   size_t *offset,
				   int32_t *decoded)
{
	*decoded = get_unaligned_le32(buffer + *offset);
	*offset += sizeof(int32_t);
}

/**
 * Encode a 32 bit signed number into a buffer at a given offset using
 * a little-endian representation. The offset will be advanced to
 * first byte after the encoded number.
 *
 * @param data      The buffer to encode into
 * @param offset    A pointer to the offset at which to start encoding
 * @param to_encode The number to encode
 **/
static INLINE void encode_int32_le(uint8_t *data,
				   size_t *offset,
				   int32_t to_encode)
{
	put_unaligned_le32(to_encode, data + *offset);
	*offset += sizeof(int32_t);
}

/**
 * Extract a 32 bit unsigned little-endian number from a buffer at a
 * specified offset.  The offset will be advanced to the first byte
 * after the number.
 *
 * @param buffer  The buffer from which to extract the number
 * @param offset  A pointer to the offset into the buffer at which to extract
 * @param decoded A pointer to hold the extracted number
 **/
static INLINE void decode_uint32_le(const uint8_t *buffer,
				    size_t *offset,
				    uint32_t *decoded)
{
	*decoded = get_unaligned_le32(buffer + *offset);
	*offset += sizeof(uint32_t);
}

/**
 * Encode a 32 bit unsigned number into a buffer at a given offset
 * using a little-endian representation. The offset will be advanced
 * to first byte after the encoded number.
 *
 * @param data      The buffer to encode into
 * @param offset    A pointer to the offset at which to start encoding
 * @param to_encode The number to encode
 **/
static INLINE void encode_uint32_le(uint8_t *data,
				    size_t *offset,
				    uint32_t to_encode)
{
	put_unaligned_le32(to_encode, data + *offset);
	*offset += sizeof(uint32_t);
}

/**
 * Extract a 16 bit unsigned little-endian number from a buffer at a
 * specified offset.  The offset will be advanced to the first byte
 * after the number.
 *
 * @param buffer  The buffer from which to extract the number
 * @param offset  A pointer to the offset into the buffer at which to
 *                extract
 * @param decoded A pointer to hold the extracted number
 **/
static INLINE void decode_uint16_le(const uint8_t *buffer,
				    size_t *offset,
				    uint16_t *decoded)
{
	*decoded = get_unaligned_le16(buffer + *offset);
	*offset += sizeof(uint16_t);
}

/**
 * Encode a 16 bit unsigned number into a buffer at a given offset
 * using a little-endian representation. The offset will be advanced
 * to first byte after the encoded number.
 *
 * @param data      The buffer to encode into
 * @param offset    A pointer to the offset at which to start encoding
 * @param to_encode The number to encode
 **/
static INLINE void encode_uint16_le(uint8_t *data,
				    size_t *offset,
				    uint16_t to_encode)
{
	put_unaligned_le16(to_encode, data + *offset);
	*offset += sizeof(uint16_t);
}

/**
 * Special function wrapper required for compile-time assertions. This
 * function will fail to compile if any of the uint*_t types are not of the
 * size we expect. This function should never be called.
 **/
void numeric_compile_time_assertions(void);

#endif /* NUMERIC_H */
