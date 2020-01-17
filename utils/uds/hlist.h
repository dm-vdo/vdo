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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/hlist.h#1 $
 */

#ifndef HLIST_H
#define HLIST_H

#include "atomicDefs.h"
#include "compiler.h"

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

#define hlist_entry_safe(ptr, type, member)                            \
  __extension__ ({ __typeof__(ptr) ____ptr = (ptr);                    \
                 ____ptr ? container_of(____ptr, type, member) : NULL; \
                })

#define hlist_for_each_entry(pos, head, member)                           \
  for (pos = hlist_entry_safe((head)->first, __typeof__(*(pos)), member); \
       pos;                                                               \
       pos = hlist_entry_safe((pos)->member.next, __typeof__(*(pos)), member))

static INLINE void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
  struct hlist_node *first = h->first;
  n->next = first;
  if (first){
    first->pprev = &n->next;
  }
  WRITE_ONCE(h->first, n);
  n->pprev = &h->first;
}

static INLINE void hlist_del(struct hlist_node *n)
{
  struct hlist_node *next = n->next;
  struct hlist_node **pprev = n->pprev;
  WRITE_ONCE(*pprev, next);
  if (next) {
    next->pprev = pprev;
  }
}

static INLINE int hlist_empty(const struct hlist_head *h)
{
  return !READ_ONCE(h->first);
}

#endif /* HLIST_H */
