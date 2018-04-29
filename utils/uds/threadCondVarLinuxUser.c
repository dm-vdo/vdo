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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/threadCondVarLinuxUser.c#1 $
 */

#include "threadCondVar.h"

#include "permassert.h"

/**********************************************************************/
int initCondAttr(pthread_condattr_t *cond_attr)
{
  int result = pthread_condattr_init(cond_attr);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond_attr: 0x%p",
                                (void *) cond_attr);
}

/**********************************************************************/
int setClockCondAttr(pthread_condattr_t *cond_attr, clockid_t clockID)
{
  int result = pthread_condattr_setclock(cond_attr, clockID);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "cond_attr: 0x%p, clockID: %d",
                                (void *) cond_attr, clockID);
}

/**********************************************************************/
int destroyCondAttr(pthread_condattr_t *cond_attr)
{
  int result = pthread_condattr_destroy(cond_attr);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond_attr: 0x%p",
                                (void *) cond_attr);
}

/**********************************************************************/
int initCondWithAttr(CondVar *cond, pthread_condattr_t *cond_attr)
{
  int result = pthread_cond_init(cond, cond_attr);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "cond: 0x%p, cond_attr: 0x%p",
                                (void *) cond, (void *) cond_attr);
}

/**********************************************************************/
int initCond(CondVar *cond)
{
  int result = pthread_cond_init(cond, NULL);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "cond: 0x%p", (void *) cond);
}

/**********************************************************************/
int signalCond(CondVar *cond)
{
  int result = pthread_cond_signal(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond: 0x%p",
                                (void *) cond);
}

/**********************************************************************/
int broadcastCond(CondVar *cond)
{
  int result = pthread_cond_broadcast(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond: 0x%p",
                                (void *) cond);
}

/**********************************************************************/
int waitCond(CondVar *cond, Mutex *mutex)
{
  int result = pthread_cond_wait(cond, mutex);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond: 0x%p",
                                (void *) cond);
}

/**********************************************************************/
int timedWaitCond(CondVar *cond, Mutex *mutex, const AbsTime *deadline)
{
  return pthread_cond_timedwait(cond, mutex, deadline);
}

/**********************************************************************/
int destroyCond(CondVar *cond)
{
  int result = pthread_cond_destroy(cond);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "cond: 0x%p",
                                (void *) cond);
}
