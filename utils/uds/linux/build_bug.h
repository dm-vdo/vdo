/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef _LINUX_BUILD_BUG_H
#define _LINUX_BUILD_BUG_H

#define BUILD_BUG_ON(condition) \
	BUILD_BUG_ON_MSG(condition, "BUILD_BUG_ON failed: " #condition)

#define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)

#define _compiletime_assert(condition, msg, prefix, suffix) \
	__compiletime_assert(condition, msg, prefix, suffix)

#define compiletime_assert(condition, msg) \
	_compiletime_assert(condition, msg, __compiletime_assert_, __COUNTER__)

#define __compiletime_assert(condition, msg, prefix, suffix) do { } while (0)

#endif /* _LINUX_BUILD_BUG_H */
