//
// Mutex_POSIX.cpp
//
// $Id: //poco/1.4/Foundation/src/Mutex_POSIX.cpp#4 $
//
// Library: Foundation
// Package: Threading
// Module:  Mutex
//
// Copyright (c) 2004-2008, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/Mutex_POSIX.h"
#include "Poco/Timestamp.h"
#if !defined(POCO_NO_SYS_SELECT_H)
#include <sys/select.h>
#endif
#include <unistd.h>
#if defined(POCO_VXWORKS)
#include <timers.h>
#include <cstring>
#else
#include <sys/time.h>
#endif


#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS - 200112L) >= 0L
#if defined(_POSIX_THREADS) && (_POSIX_THREADS - 200112L) >= 0L
#define POCO_HAVE_MUTEX_TIMEOUT
#endif
#endif


namespace Poco {


MutexImpl::MutexImpl(MutexTypeImpl type)
{
#if defined(POCO_VXWORKS)
	// This workaround is for VxWorks 5.x where
	// pthread_mutex_init() won't properly initialize the mutex
	// resulting in a subsequent freeze in pthread_mutex_destroy()
	// if the mutex has never been used.
	std::memset(&_mutex, 0, sizeof(_mutex));
#endif
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
#if defined(PTHREAD_MUTEX_RECURSIVE_NP)
	pthread_mutexattr_settype_np(&attr, type == MUTEX_RECURSIVE_IMPL ? PTHREAD_MUTEX_RECURSIVE_NP : PTHREAD_MUTEX_NORMAL_NP);
#elif !defined(POCO_VXWORKS)
	pthread_mutexattr_settype(&attr, type == MUTEX_RECURSIVE_IMPL ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
#endif
	if (pthread_mutex_init(&_mutex, &attr))
	{
		pthread_mutexattr_destroy(&attr);
		throw SystemException("cannot create mutex");
	}
	pthread_mutexattr_destroy(&attr);
}


MutexImpl::~MutexImpl()
{
	pthread_mutex_destroy(&_mutex);
}


bool MutexImpl::tryLockImpl(long milliseconds)
{
#if defined(POCO_HAVE_MUTEX_TIMEOUT)
	struct timespec abstime;
#if defined(_POSIX_TIMERS) && defined(CLOCK_REALTIME)
	clock_gettime(CLOCK_REALTIME, &abstime);
	abstime.tv_sec  += milliseconds / 1000;
	abstime.tv_nsec += (milliseconds % 1000)*1000000;
	if (abstime.tv_nsec >= 1000000000)
	{
		abstime.tv_nsec -= 1000000000;
		abstime.tv_sec++;
	}
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	abstime.tv_sec  = tv.tv_sec + milliseconds / 1000;
	abstime.tv_nsec = tv.tv_usec*1000 + (milliseconds % 1000)*1000000;
	if (abstime.tv_nsec >= 1000000000)
	{
		abstime.tv_nsec -= 1000000000;
		abstime.tv_sec++;
	}
#endif
	int rc = pthread_mutex_timedlock(&_mutex, &abstime);
	if (rc == 0)
		return true;
	else if (rc == ETIMEDOUT)
		return false;
	else
		throw SystemException("cannot lock mutex");
#else
	const int sleepMillis = 5;
	Timestamp now;
	Timestamp::TimeDiff diff(Timestamp::TimeDiff(milliseconds)*1000);
	do
	{
		int rc = pthread_mutex_trylock(&_mutex);
		if (rc == 0)
			return true;
		else if (rc != EBUSY)
			throw SystemException("cannot lock mutex");
#if defined(POCO_VXWORKS)
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = sleepMillis*1000000;
		nanosleep(&ts, NULL);
		
#else
		struct timeval tv;
		tv.tv_sec  = 0;
		tv.tv_usec = sleepMillis * 1000;
		select(0, NULL, NULL, NULL, &tv);
#endif
	}
	while (!now.isElapsed(diff));
	return false;
#endif
}


FastMutexImpl::FastMutexImpl(): MutexImpl(MUTEX_NONRECURSIVE_IMPL)
{
}


FastMutexImpl::~FastMutexImpl()
{
}


} // namespace Poco
