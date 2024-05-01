/*
 * Unit test requirements from linux/blkdev.h and other kernel headers.
 */

#ifndef LINUX_BLKDEV_H
#define LINUX_BLKDEV_H

#include <linux/compiler_attributes.h>
#include <linux/types.h>
#include <stdio.h>

#define SECTOR_SHIFT    9
#define SECTOR_SIZE   512
#define BDEVNAME_SIZE  32 /* Largest string for a blockdev identifier */

/* Defined in linux/kdev_t.h */
#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

#define MAJOR(dev) ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int) ((dev) & MINORMASK))

#define format_dev_t(buffer, dev)				\
	sprintf(buffer, "%u:%u", MAJOR(dev), MINOR(dev))

/* Defined in linux/fs.h but hacked for vdo unit testing */
struct inode {
  loff_t size;
};

/* Defined in linux/blk_types.h */
typedef unsigned int blk_opf_t;
typedef uint32_t blk_status_t;
typedef unsigned int blk_qc_t;

struct bio;

struct block_device {
	int fd;
	dev_t bd_dev;

	/* This is only here for i_size_read(). */
	struct inode *bd_inode;
};

/**********************************************************************/
static inline int blk_status_to_errno(blk_status_t status)
{
  return (int) status;
}

/**********************************************************************/
static inline blk_status_t errno_to_blk_status(int error)
{
  return (blk_status_t) error;
}

/**********************************************************************/
blk_qc_t submit_bio_noacct(struct bio *bio);

/* Defined in linux/fs.h, but it's convenient to implement here. */
static inline loff_t i_size_read(const struct inode *inode)
{
        return (inode == NULL) ? (loff_t) SIZE_MAX : inode->size;
}

#endif // LINUX_BLKDEV_H
