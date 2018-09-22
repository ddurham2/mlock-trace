# mlock-trace
An LD_PRELOAD based hook for leak detection and tracing of mlock() and munlock() on Linux

Frankly, something like this should exist in valgrind too.

**Build:**
- mkdir build
- cd build
- cmake ..
- make
- make run

**Usage:** After building the user program with debug info...
- basic usage:
> LD_PRELOAD=$PWD/libmlock-trace.so **<program...>**
- demangle symbols
> LD_PRELOAD=$PWD/libmlock-trace.so **<program...>** |& c++filt
- filenames, line numbers, demangled symbols with addresses: (and plenty expensive)
> LD_PRELOAD=$PWD/libmlock-trace.so **<program...>** 2>&1 | sed -Ee 's^(.* \\[)(0x[a-f0-9]+)(])$^addr2line -apfCe **\<program\>** \2^e'
- You may send SIGUSR1 to a running process to cause it to print its currently held mlock()'s
- **IMPORTANT**: glibc's backtrace facility will not be able to locate symbols unless your application is compiled with -g and linked with -rdynamic.  If you're going to translate these into names later with an external tool, then -rdynamic would not be required.

**Useful Env Vars:**
- **MLOCK_TRACE_DEPTH=[int]** - sets the maximum stack frames recorded (default is 15)
- **MLOCK_TRACE_ACTIVE=[0|1]** - enable/disable active printing of calls to mlock()/munlock() (default is 0)
- **MLOCK_TRACE_STREAM=[stdout|stderr]** - sets the logging stream to either standard output or standard error (stderr is the default)
- **MLOCK_TRACE_SIGNUM=[int]** - sets the signal number which will trigger a dump of all currently held mlocks() (SIGUSR1 is the default)
- **MLOCK_TRACE_PRELOAD_TTL=[int]** - decremented when the process starts.  When <= 0, then LD_PRELOAD is unset to avoid its inheritance by child processes (default is 0)

