// Part of mlock-trace project
// Licensed under the BSD 2-Clause License (see LICENSE)
// Copyright 2018 Davy Durham
//
//  Build:
//  	g++ -std=c++11 -fPIC -shared mlock-trace.cpp -o libmlock-trace.so -ldl -lpthread
//

#if !defined(_GNU_SOURCE)
//#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

// from man page "Note: currently, there is not a glibc wrapper for mlock2(), so it will need to be invoked using syscall(2)."
//#define MLOCK2_DEF

static std::mutex gMutex;
static std::map<std::pair<const void*, size_t>, std::pair<std::string/*lock call*/, std::string/*call stack at lock call*/> > gMemLocks;
static const int gDepth = getenv("MLOCK_TRACE_DEPTH") ? atoi(getenv("MLOCK_TRACE_DEPTH")) : 15;
static const bool gDumpActively = getenv("MLOCK_TRACE_ACTIVE") ? atoi(getenv("MLOCK_TRACE_ACTIVE")) : 0;
static FILE* const gDumpStream = getenv("MLOCK_TRACE_STREAM") ? (strcmp(getenv("MLOCK_TRACE_STREAM"), "stdout") == 0 ? stdout : stderr) : stderr;
static const int gPreloadTTL = std::max(0, getenv("MLOCK_TRACE_PRELOAD_TTL") ? atoi(getenv("MLOCK_TRACE_PRELOAD_TTL")) - 1 : 0);


std::string get_stack_trace_symbols(unsigned skip, int maxDepth)
{
	void* array[50];
	int depth = sizeof(array) / sizeof(*array);
	if (maxDepth >= 0 && ((int)skip + maxDepth) < depth) {
		depth = skip + maxDepth;
	}

	int size = backtrace(array, depth);
	char** strings = backtrace_symbols(array, size);

	std::string r;
	for (int i = skip; i < size; i++) {
		r += "\t";
		r += strings[i];
		r += "\n";
	}
	free(strings);

	return r;
}

// from man page "Note: currently, there is not a glibc wrapper for mlock2(), so it will need to be invoked using syscall(2)."
//#define MLOCK2_DEF

static int (*real_mlock)(const void*, size_t) = NULL;
#if defined(MLOCK2_DEF)
static int (*real_mlock2)(const void*, size_t, int) = NULL;
#endif
static int (*real_munlock)(const void*, size_t) = NULL;

static bool gDumpLocks = false;

static void dump_sighandler(int)
{
	gDumpLocks = true;
}

static void dump_locks()
{
	fprintf(gDumpStream, "==== currently held memory locks ====\n");
	for (auto&& i: gMemLocks) {
		fprintf(gDumpStream, "%s\n%s\n", i.second.first.c_str(), i.second.second.c_str());
	}
	fflush(gDumpStream);
}

static int trace_init()
{
	real_mlock = (decltype(real_mlock))dlsym(RTLD_NEXT, "mlock");
	if (!real_mlock) {
		fprintf(gDumpStream, "error loading real mlock symbol -- %s\n", dlerror());fflush(gDumpStream);
		abort();

	}

#if defined(MLOCK2_DEF)
	real_mlock2 = (decltype(real_mlock2))dlsym(RTLD_NEXT, "mlock2");
	if (!real_mlock2) {
		fprintf(gDumpStream, "error loading real mlock2 symbol -- %s\n", dlerror());fflush(gDumpStream);
		abort();
	}
#endif

	real_munlock = (decltype(real_munlock))dlsym(RTLD_NEXT, "munlock");
	if (!real_munlock) {
		fprintf(gDumpStream, "error loading real munlock symbol -- %s\n", dlerror());fflush(gDumpStream);
		abort();
	}


	// arrange to print whenever SIGUSR1 is received
	std::thread thr([]() {
		for (;;) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (gDumpLocks) {
				gDumpLocks = false;
				dump_locks();
			}
		}
	});
	thr.detach();
	signal(getenv("MLOCK_TRACE_SIGNUM") ? atoi(getenv("MLOCK_TRACE_SIGNUM")) : SIGUSR1, dump_sighandler);

	// arrange to print at exit
	atexit(dump_locks);

	// if this counts down to zero, then don't let LD_PRELOAD be inherited by future child processes
	if (gPreloadTTL <= 0) {
		unsetenv("LD_PRELOAD");
	}

	return 1;
}

static const int initialized = trace_init();

static long get_thread_id()
{
	return syscall(SYS_gettid);
}

extern "C" int mlock(const void* addr, size_t len)
{
	std::unique_lock<std::mutex> ml(gMutex);

	if (!real_mlock) {
		trace_init();
	}


	char call[256];
	sprintf(call, "mlock(%p, %zu) (locking thread: %ld)", addr, len, get_thread_id());

	auto k = std::make_pair(addr, len);

	gMemLocks[k] = std::make_pair<std::string, std::string>(call, get_stack_trace_symbols(2, gDepth));

	if (gDumpActively) {
		fprintf(gDumpStream, "LOCK - %s\n%s\n", call, gMemLocks[k].second.c_str());
	}

	return real_mlock(addr, len);
}

#if defined(MLOCK2_DEF)
extern "C" int mlock2(const void* addr, size_t len, int flags)
{
	std::unique_lock<std::mutex> ml(gMutex);

	if (!real_mlock2) {
		trace_init();
	}

	char call[256];
	sprintf(call, "mlock2(%p, %zu, %x) (locking thread: %ld)", addr, len, flags, get_thread_id());

	auto k = std::make_pair(addr, len);

	gMemLocks[k] = std::make_pair<std::string, std::string>(call, get_stack_trace_symbols(2, gDepth));

	if (gDumpActively) {
		fprintf(gDumpStream, "LOCK - %s\n%s\n", call, gMemLocks[k].second.c_str());
	}

	return real_mlock2(addr, len, flags);
}
#endif

extern "C" int munlock(const void* addr, size_t len)
{
	std::unique_lock<std::mutex> ml(gMutex);

	if (!real_munlock) {
		trace_init();
	}

	auto k = std::make_pair(addr, len);
	auto&& i = gMemLocks.find(k);

	if (gDumpActively) {
		char call[256];
		sprintf(call, "munlock(%p, %zu) (unlocking thread: %ld)", addr, len, get_thread_id());

		if (i == gMemLocks.end()) {
			fprintf(gDumpStream, "UNLOCK - WITH NO MATCHING CALL TO mlock()\n");
		} else {
			fprintf(gDumpStream, "UNLOCK - %s\n%s\n", call, i->second.second.c_str());
		}

	}
	if (i != gMemLocks.end()) {
		gMemLocks.erase(i);
	}

	return real_munlock(addr, len);
}


