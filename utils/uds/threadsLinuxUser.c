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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/threadsLinuxUser.c#4 $
 */

#include "threads.h"

#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utmpx.h>

#include "compiler.h"
#include "cpu.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "murmur/MurmurHash3.h"
#include "permassert.h"
#include "syscalls.h"
#include "uds.h"

static UdsThreadStartHook *threadStartHook = NULL;

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
unsigned int getScheduledCPU(void)
{
  int cpu = sched_getcpu();
  if (likely(cpu >= 0)) {
    return cpu;
  }
  // The only error sched_getcpu can return is ENOSYS, meaning that the
  // kernel does not implement getcpu(), in which case return a usable
  // hint.
  int result __attribute__((unused))
    = ASSERT_WITH_ERROR_CODE(cpu >= 0, errno, "sched_getcpu failed");

  // Hash the POSIX thread identifier. (We could use a random number instead.)
  pthread_t threadID = pthread_self();
  uint32_t hashCode;
  MurmurHash3_x86_32(&threadID, sizeof(threadID), 0, &hashCode);
  // Should probably get the total number of CPUs into a static instead, but
  // we shouldn't really be on this code path.
  return (hashCode % countAllCores());
}

/**********************************************************************/
unsigned int countAllCores(void)
{
  int result = sysconf(_SC_NPROCESSORS_CONF);
  // Treat zero as an erroneous result; how can we have no cores?
  if (result <= 0) {
    logWarningWithStringError(result,
                              "sysconf(_SC_NPROCESSORS_CONF) failed,"
                              " returning 2 as total number of cores");
    return 2;
  }
  return result;
}

/**********************************************************************/
void getThreadName(char *name)
{
  processControl(PR_GET_NAME, (unsigned long) name, 0, 0, 0);
}

/**********************************************************************/
ThreadId getThreadId(void)
{
  return (ThreadId) syscall(SYS_gettid);
}

/**********************************************************************/
UdsThreadStartHook *udsSetThreadStartHook(UdsThreadStartHook *hook)
{
  UdsThreadStartHook *oldUdsThreadStartHook = threadStartHook;
  threadStartHook = hook;
  return oldUdsThreadStartHook;
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
  if (threadStartHook != NULL) {
    (*threadStartHook)();
  }
  threadFunc(threadData);
  return NULL;
}

/**********************************************************************/
int createThread(void      (*threadFunc)(void *),
                 void       *threadData,
                 const char *name,
                 pthread_t  *newThread)
{
  ThreadStartInfo *tsi;
  int result = ALLOCATE(1, ThreadStartInfo, __func__, &tsi);
  if (result != UDS_SUCCESS) {
    return result;
  }
  tsi->threadFunc = threadFunc;
  tsi->threadData = threadData;
  tsi->name       = name;

  result = pthread_create(newThread, NULL, threadStarter, tsi);
  if (result != 0) {
    logErrorWithStringError(errno, "could not create %s thread", name);
    FREE(tsi);
    return UDS_ENOTHREADS;
  }
  return UDS_SUCCESS;
}

/**********************************************************************/
int joinThreads(pthread_t th)
{
  int result = pthread_join(th, NULL);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "th: %zu", th);
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
int initializeBarrier(Barrier *barrier, unsigned int threadCount)
{
  int result = pthread_barrier_init(barrier, NULL, threadCount);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_barrier_init error");
}

/**********************************************************************/
int destroyBarrier(Barrier *barrier)
{
  int result = pthread_barrier_destroy(barrier);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "pthread_barrier_destroy error");
}

/**********************************************************************/
int enterBarrier(Barrier *barrier, bool *winner)
{
  int result = pthread_barrier_wait(barrier);

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

/**********************************************************************/
int initializeSynchronousRequest(SynchronousCallback *callback)
{
  int result = initCond(&callback->condition);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = initMutex(&callback->mutex);
  if (result != UDS_SUCCESS) {
    destroyCond(&callback->condition);
    return result;
  }
  callback->complete = false;
  return UDS_SUCCESS;
}

/**********************************************************************/
void awaitSynchronousRequest(SynchronousCallback *callback)
{
  lockMutex(&callback->mutex);
  while (!callback->complete) {
    waitCond(&callback->condition, &callback->mutex);
  }
  unlockMutex(&callback->mutex);
  destroyCond(&callback->condition);
  destroyMutex(&callback->mutex);
}

/**********************************************************************/
void awakenSynchronousRequest(SynchronousCallback *callback)
{
  lockMutex(&callback->mutex);
  callback->complete = true;
  broadcastCond(&callback->condition);
  unlockMutex(&callback->mutex);
}
