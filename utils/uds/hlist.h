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

#ifndef HLIST_H
#define HLIST_H

#include <linux/atomic.h>

/*
 * An "hlist" is a doubly linked list with the listhead being a single pointer
 * to the head of the list.
 *
 * The Linux kernel provides an hlist implementation in <linux/list.h>.  This
 * file defines the hlist interfaces used by UDS for the user mode build.
 *
 * The equivalent used in the user <sys/queue.h> implementation is a LIST.
 */

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_entry_safe(ptr, type, member)				     \
	__extension__({ typeof(ptr) ____ptr = (ptr);			     \
			____ptr ? hlist_entry(____ptr, type, member) : NULL; \
		})

/**
 * Iterate over list of given type
 * @param pos	 the type * to use as a loop cursor.
 * @param head	 the head for your list.
 * @param member the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(pos, head, member)				    \
	for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
	     pos;							    \
	     pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * Add a new entry at the beginning of the hlist
 * @param n new entry to be added
 * @param h hlist head to add it after
 */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	WRITE_ONCE(n->next, first);
	if (first)
		WRITE_ONCE(first->pprev, &n->next);
	WRITE_ONCE(h->first, n);
	WRITE_ONCE(n->pprev, &h->first);
}

/**
 * Delete the specified hlist_node from its list
 * @param n Node to delete.
 */
static inline void hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	WRITE_ONCE(*pprev, next);
	if (next)
		WRITE_ONCE(next->pprev, pprev);
}

/**
 * Is the specified hlist_head structure an empty hlist?
 * @param h Structure to check.
 */
static inline int hlist_empty(const struct hlist_head *h)
{
	return !READ_ONCE(h->first);
}

#endif /* HLIST_H */
