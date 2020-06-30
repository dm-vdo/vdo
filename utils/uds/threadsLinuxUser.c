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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/threadsLinuxUser.c#3 $
 */

#include "threads.h"

#include <errno.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "syscalls.h"

/**********************************************************************/
unsigned int getNumCores(void)
{
  cpu_set_t cpuSet;
  if (sched_getaffinity(0, sizeof(cpuSet), &cpuSet) != 0) {
    logWarningWithStringError(errno,
                              "schedGetAffinity() failed, using 1 "
                              "as number of cores.");
    return 1;
  }

  unsigned int nCpus = 0;
  for (unsigned int i = 0; i < CPU_SETSIZE; ++i) {
    nCpus += CPU_ISSET(i, &cpuSet);
  }
  return nCpus;
}

/**********************************************************************/
void getThreadName(char *name)
{
  processControl(PR_GET_NAME, (unsigned long) name, 0, 0, 0);
}

/**********************************************************************/
pid_t get_thread_id(void)
{
  return syscall(SYS_gettid);
}

/**********************************************************************/
typedef struct {
  void (*threadFunc)(void *);
  void *threadData;
  const char *name;
} ThreadStartInfo;

/**********************************************************************/
static void *threadStarter(void *arg)
{
  ThreadStartInfo *tsi = arg;
  void (*threadFunc)(void *) = tsi->threadFunc;
  void *threadData = tsi->threadData;
  /*
   * The name is just advisory for humans examining it, so we don't
   * care much if this fails.
   */
  processControl(PR_SET_NAME, (unsigned long) tsi->name, 0, 0, 0);
  FREE(tsi);
  threadFunc(threadData);
  return NULL;
}

/**********************************************************************/
int createThread(void          (*threadFunc)(void *),
                 void           *threadData,
                 const char     *name,
                 struct thread **newThread)
{
  ThreadStartInfo *tsi;
  int result = ALLOCATE(1, ThreadStartInfo, __func__, &tsi);
  if (result != UDS_SUCCESS) {
    return result;
  }
  tsi->threadFunc = threadFunc;
  tsi->threadData = threadData;
  tsi->name       = name;

  struct thread *thread;
  result = ALLOCATE(1, struct thread, __func__, &thread);
  if (result != UDS_SUCCESS) {
    logWarning("Error allocating memory for %s", name);
    FREE(tsi);
    return result;
  }

  result = pthread_create(&thread->thread, NULL, threadStarter, tsi);
  if (result != 0) {
    logErrorWithStringError(errno, "could not create %s thread", name);
    FREE(thread);
    FREE(tsi);
    return UDS_ENOTHREADS;
  }
  *newThread = thread;
  return UDS_SUCCESS;
}

/**********************************************************************/
int joinThreads(struct thread *th)
{
  int result = pthread_join(th->thread, NULL);
  pthread_t pthread = th->thread;
  FREE(th);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "th: %zu", pthread);
}

/**********************************************************************/
int createThreadKey(pthread_key_t *key,
                    void (*destr_function) (void *) )
{
  int result = pthread_key_create(key, destr_function);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_key_create error");
}

/**********************************************************************/
int deleteThreadKey(pthread_key_t key)
{
  int result = pthread_key_delete(key);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_key_delete error");
}

/**********************************************************************/
int setThreadSpecific(pthread_key_t key, const void *pointer)
{
  int result = pthread_setspecific(key, pointer);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_setspecific error");
}

/**********************************************************************/
void *getThreadSpecific(pthread_key_t key)
{
  return pthread_getspecific(key);
}

/**********************************************************************/
int initializeBarrier(struct barrier *barrier, unsigned int threadCount)
{
  int result = pthread_barrier_init(&barrier->barrier, NULL, threadCount);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_barrier_init error");
}

/**********************************************************************/
int destroyBarrier(struct barrier *barrier)
{
  int result = pthread_barrier_destroy(&barrier->barrier);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_barrier_destroy error");
}

/**********************************************************************/
int enterBarrier(struct barrier *barrier, bool *winner)
{
  int result = pthread_barrier_wait(&barrier->barrier);

  // Check if this thread is the arbitrary winner and pass that result back as
  // an optional flag instead of overloading the return value.
  if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
    if (winner != NULL) {
      *winner = true;
    }
    return UDS_SUCCESS;
  }

  if (winner != NULL) {
    *winner = false;
  }
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_barrier_wait error");
}

/**********************************************************************/
int yieldScheduler(void)
{
  int result = sched_yield();
  if (result != 0) {
    return logErrorWithStringError(errno, "sched_yield failed");
  }

  return UDS_SUCCESS;
}
