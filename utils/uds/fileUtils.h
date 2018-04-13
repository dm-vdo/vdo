/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileUtils.h#1 $
 */

#ifndef FILE_UTILS_H
#define FILE_UTILS_H 1

#include <sys/stat.h>

#include "accessMode.h"
#include "common.h"
#include "compiler.h"
#include "typeDefs.h"

typedef enum {
  FU_READ_ONLY         = 0, // open file with read-only access
  FU_READ_WRITE        = 1, // open file with read-write access
  FU_CREATE_READ_WRITE = 2, // same, but create and truncate with 0666 mode
                            // bits if the file doesn't exist
  FU_CREATE_WRITE_ONLY = 3, // like above, but open for writing only
                                   // Direct I/O:
  FU_READ_ONLY_DIRECT         = 4, // open file with read-only acces
  FU_READ_WRITE_DIRECT        = 5, // open file with read-write access
  FU_CREATE_READ_WRITE_DIRECT = 6, // same, but create and truncate with
                                   // 0666 mode bits if the file doesn't exist
  FU_CREATE_WRITE_ONLY_DIRECT = 7, // like above, but open for writing only
} FileAccess;

/*****************************************************************************/
static INLINE FileAccess fileAccessMode(IOAccessMode access)
{
  return
    (access == IO_CREATE_WRITE) ? FU_CREATE_WRITE_ONLY :
    (access == IO_READ_WRITE)   ? FU_READ_WRITE :
    (access & IO_CREATE)        ? FU_CREATE_READ_WRITE :
    FU_READ_ONLY;
}

/**
 * Determine the best size for IO buffers.
 *
 * @param defaultSize The default size to use if a better size can't be
 *                    determined
 * @param blockSize   A pointer to hold basic block size of the system
 * @param bestSize    A pointer to hold the best buffer size.
 *
 * @return UDS_SUCCESS or an error code
 **/
int getBufferSizeInfo(size_t defaultSize, size_t *blockSize, size_t *bestSize)
  __attribute__((warn_unused_result));

/**
 * Check whether a file exists.
 *
 * @param path   The path to the file
 * @param exists A pointer to a bool which will be set to true if the
 *               file exists and false if it does not.
 *
 * @return UDS_SUCCESS or an error code
 **/
int fileExists(const char *path, bool *exists)
  __attribute__((warn_unused_result));

/**
 * Open a file.
 *
 * @param path   The path to the file
 * @param access Access mode selected
 * @param fd     A pointer to the return file descriptor on success
 *
 * @return UDS_SUCCESS or an error code
 **/
int openFile(const char *path, FileAccess access, int *fd)
  __attribute__((warn_unused_result));

/**
 * Close a file.
 *
 * @param fd           The file descriptor to close
 * @param errorMessage The error message to log if the close fails (if
 *                     <code>NULL</code>, no error will be logged).
 *
 * @return UDS_SUCCESS          on success
 *         UDS_EIO              if there was an I/O error closing the file
 *         UDS_ASSERTION_FAILED if there was any other error
 **/
int closeFile(int fd, const char *errorMessage)
  __attribute__((warn_unused_result));

/**
 * Attempt to close a file, ignoring errors.
 *
 * @param fd  The file descriptor to close
 **/
void tryCloseFile(int fd);

/**
 * Close a file after syncing it.
 *
 * @param fd           The file descriptor to close
 * @param errorMessage The error message to log if the close fails (if
 *                     <code>NULL</code>, no error will be logged).
 *
 * @return UDS_SUCCESS          on success
 *         UDS_EIO              if there was an I/O error syncing or closing
 *                              the file
 *         UDS_ASSERTION_FAILED if there was any other error
 **/
int syncAndCloseFile(int fd, const char *errorMessage)
  __attribute__((warn_unused_result));

/**
 * Attempt to sync and then close a file, ignoring errors.
 *
 * @param fd  The file descriptor to sync and close
 **/
void trySyncAndCloseFile(int fd);

/**
 * Read into a buffer from a file.
 *
 * @param fd     The file descriptor from which to read
 * @param buffer The buffer into which to read
 * @param length The number of bytes to read
 *
 * @return UDS_SUCCESS or an error code
 **/
int readBuffer(int fd, void *buffer, unsigned int length)
  __attribute__((warn_unused_result));

/**
 * Read into a buffer from a file at a given offset into the file
 *
 * @param fd     The file descriptor from which to read
 * @param offset The file offset at which to start reading
 * @param buffer The buffer into which to read
 * @param length The number of bytes to read
 *
 * @return UDS_SUCCESS or an error code
 **/
int readBufferAtOffset(int fd, off_t offset, void *buffer, unsigned int length)
  __attribute__((warn_unused_result));

/**
 * Read into a buffer from a file at a given offset into the file.
 * Unlike readBufferAtOffset{,NonCritical} this variant allows
 * short reads as long some data is read.
 *
 * @param [in]  fd      The file descriptor from which to read
 * @param [in]  offset  The file offset at which to start reading
 * @param [in]  buffer  The buffer into which to read
 * @param [in]  size    The size of the buffer
 * @param [out] length  The amount actually read.
 *
 * @return UDS_SUCCESS or an error code, UDS_END_OF_FILE if amount
 *      read is zero
 **/
int readDataAtOffset(int           fd,
                     off_t         offset,
                     void         *buffer,
                     unsigned int  size,
                     unsigned int *length)
  __attribute__((warn_unused_result));

/**
 * Read a given number of bytes from a file descriptor and check whether
 * they match expectations.
 *
 * @param fd            The file descriptor from which to read
 * @param requiredValue The expectation
 * @param length        The number of bytes to read and compare
 *
 * @return UDS_SUCCESS or an error code
 **/
int readAndVerify(int fd, const byte *requiredValue, unsigned int length)
  __attribute__((warn_unused_result));

/**
 * Write a buffer to a file.
 *
 * @param fd     The file descriptor to which to write
 * @param buffer The buffer to write
 * @param length The number of bytes to write
 *
 * @return UDS_SUCCESS or an error code
 **/
int writeBuffer(int fd, const void *buffer, unsigned int length)
  __attribute__((warn_unused_result));

/**
 * Write a buffer to a file starting at a given offset in the file.
 *
 * @param fd     The file descriptor to which to write
 * @param offset The offset into the file at which to write
 * @param buffer The buffer to write
 * @param length The number of bytes to write
 *
 * @return UDS_SUCCESS or an error code
 **/
int writeBufferAtOffset(int           fd,
                        off_t         offset,
                        const void   *buffer,
                        unsigned int  length)
  __attribute__((warn_unused_result));

/**
 * Determine the size of an open file.
 *
 * @param fd        the file descriptor
 * @param sizePtr   a pointer in which to store the result
 *
 * @return UDS_SUCCESS or an error code
 **/
int getOpenFileSize(int fd, off_t *sizePtr)
  __attribute__((warn_unused_result));

/**
 * Truncate and/or extend an open file to be the specified size.
 *
 * @param fd        the file descriptor
 * @param size      the requested size
 *
 * @return UDS_SUCCESS or an error code
 **/
int setOpenFileSize(int fd, off_t size);

/**
 * Remove a file, logging an error if any.
 *
 * @param fileName      The file name to remove
 *
 * @return              UDS_SUCCESS or error code.
 **/
int removeFile(const char *fileName);

/**
 * Match file or path name.
 *
 * @param pattern       A shell wildcard pattern.
 * @param string        String to match against pattern.
 * @param flags         Modify matching behavior as per fnmatch(3).
 *
 * @return              True if there was a match, false otherwise.
 *
 * @note                Logs errors encountered.
 **/
bool fileNameMatch(const char *pattern, const char *string, int flags)
  __attribute__((warn_unused_result));

/**
 * Convert a path to an absolute path by adding the current working directory
 * to the beginning if necessary. On success, <tt>absPath</tt> should be
 * freed by the caller.
 *
 * @param [in]  path           A path to be converted
 * @param [out] absPath        An absolute path
 *
 * @return UDS_SUCCESS or an error code
 **/
int makeAbsPath(const char *path, char **absPath);

/**
 * Wrap the stat(2) system call.
 *
 * @param path    The path to stat
 * @param buf     A buffer to hold the stat results
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingStat(const char *path, struct stat *buf, const char *context)
  __attribute__((warn_unused_result));

/**
 * Wrap the stat(2) system call. Use this version if it should not be an
 * error for the file being statted to not exist.
 *
 * @param path    The path to stat
 * @param buf     A buffer to hold the stat results
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingStatMissingOk(const char  *path,
                         struct stat *buf,
                         const char  *context)
  __attribute__((warn_unused_result));

/**
 * Wrap the fstat(2) system call.
 *
 * @param fd      The descriptor to stat
 * @param buf     A buffer to hold the stat results
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingFstat(int fd, struct stat *buf, const char *context)
  __attribute__((warn_unused_result));

/**
 * Wrap the fcntl(2) system call.
 *
 * @param fd      The descritor on which to operate
 * @param cmd     The command to apply (see the fcntl(2) man page)
 * @param context The calling context (for logging)
 * @param value   A pointer to hold the fcntl() return data
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingFcntl(int fd, int cmd, const char *context, long *value)
  __attribute__((warn_unused_result));

/**
 * Wrap the fcntl(2) system call.
 *
 * @param fd      The descritor on which to operate
 * @param cmd     The command to apply (see the fcntl(2) man page)
 * @param arg     The fcntl command argument (see the fcntl(2) man
 *                page)
 * @param context The calling context (for logging)
 * @param value   A pointer to hold the fcntl() return data
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingFcntlWithArg(int         fd,
                        int         cmd,
                        long        arg,
                        const char *context,
                        long       *value)
  __attribute__((warn_unused_result));

/**
 * Wrap the fsync(2) system call.
 *
 * @param fd      The descriptor to sync
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingFsync(int fd, const char *context)
  __attribute__((warn_unused_result));

/**
 * Wrap the lseek(2) system call.
 *
 * @param fd        The descriptor to seek
 * @param offset    The offset to which to see
 * @param whence    The offset directive (see the lseek(2) man page)
 * @param context   The calling context (for logging)
 * @param offsetPtr A pointer to hold the new file offset (may be NULL)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingLseek(int         fd,
                 off_t       offset,
                 int         whence,
                 const char *context,
                 off_t      *offsetPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the mkdir(2) system call.
 *
 * @param path          The path of the directory to make
 * @param mode          The file mode of the new directory
 * @param directoryType The type of directory (for error reporting)
 * @param context       The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int makeDirectory(const char *path,
                  mode_t      mode,
                  const char *directoryType,
                  const char *context)
  __attribute__((warn_unused_result));

/**
 * Wrap the pathconf(2) system call.
 *
 * @param path    The path of the file
 * @param name    The name of the configuration to get for the file
 * @param context The calling context (for logging)
 * @param value   A pointer to hold the option value
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingPathconf(const char *path,
                    int         name,
                    const char *context,
                    long       *value)
  __attribute__((warn_unused_result));

/**
 * Wrap the rename(2) system call.
 *
 * @param oldPath The path to rename
 * @param newPath The new name
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingRename(const char *oldPath,
                  const char *newPath,
                  const char *context)
  __attribute__((warn_unused_result));

#endif /* FILE_UTILS_H */
