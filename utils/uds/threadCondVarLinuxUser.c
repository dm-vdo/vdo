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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/threadCondVarLinuxUser.c#1 $
 */

#include "permassert.h"
#include "threads.h"

/**********************************************************************/
int initCond(CondVar *cond)
{
  int result = pthread_cond_init(cond, NULL);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_cond_init error");
}

/**********************************************************************/
int signalCond(CondVar *cond)
{
  int result = pthread_cond_signal(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_cond_signal error");
}

/**********************************************************************/
int broadcastCond(CondVar *cond)
{
  int result = pthread_cond_broadcast(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_cond_broadcast error");
}

/**********************************************************************/
int waitCond(CondVar *cond, Mutex *mutex)
{
  int result = pthread_cond_wait(cond, mutex);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_cond_wait error");
}

/**********************************************************************/
int timedWaitCond(CondVar *cond, Mutex *mutex, RelTime timeout)
{
  struct timespec ts = asTimeSpec(futureTime(CLOCK_REALTIME, timeout));
  return pthread_cond_timedwait(cond, mutex, &ts);
}

/**********************************************************************/
int destroyCond(CondVar *cond)
{
  int result = pthread_cond_destroy(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_cond_destroy error");
}
