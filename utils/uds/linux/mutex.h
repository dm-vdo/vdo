/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrap our own mutexes to mimic the kernel.
 *
 * Copyright 2023 Red Hat
 *
 */

#ifndef LINUX_MUTEX_H
#define LINUX_MUTEX_H

#include "thread-utils.h"

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname = UDS_MUTEX_INITIALIZER

#define mutex_destroy(mutex) uds_destroy_mutex(mutex)
#define mutex_init(mutex) \
	VDO_ASSERT_LOG_ONLY(uds_init_mutex(mutex) == UDS_SUCCESS, \
			    "mutex init succeeds")
#define mutex_lock(mutex) uds_lock_mutex(mutex)
#define mutex_unlock(mutex) uds_unlock_mutex(mutex)

#endif // LINUX_MUTEX_H
