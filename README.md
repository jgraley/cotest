# Cotest

Cotest is an extension of Google Test with fully integrated support for coroutines:
 - as contexts within which to run checking code and actions, 
 - and to execute the code under test.

Cotest allows for a linear test case layout, in which the test coroutine launches the code under test, and then accepts and returns mock calls in a single sequence 
of C++ statements. Cotest can do this without needing a data structure populated with expected future events. The user directly implements the test coroutine 
and can insert arbitrary logic to decide how to respond to mock calls and results from code under test, without needing to split the code into multiple lambda actions.

Cotest supports matching on both _watches_ (similar to GMock expectations) and inside the coroutine body 
(a test coroutine can _drop_ a mock call and GMock's matching search will continue). Cotest actions are coded directly into the coroutine body by the user. 
Cardinality is inferred from the execution state of the coroutine.
  
Cotest supports multiple test coroutines: these will _see_ mock calls with priority established by their watches, which may be interleaved with GMock expectations.
Cotest permits multiple overlapping launches of code under test: in the coroutine model there is no freely concurrent execution, 
but a test coroutine can control the order in which mock calls are allowed to return.
 
This project is a fork of Google Test, and changes to gtest/gmock source code have been kept minimal. C++14.

## Development status

Cotest supports test coroutines, mocking integrated with Google Mock, and launch coroutines. This completes development phases 1 and 2, bringing Cotest to concept demo status. [Three further phases](https://github.com/jgraley/cotest/milestones) remain:

 3. Test failure reporting: non-fatal failures integrated into Google Mock, more file/line info, and fixes to cardinality messages.
    - Also exceptions to/from code-under-test.
 4. Alternate coroutine backends, to include boost coroutines and maybe C++20. Cotest currently uses coroutines built on threads.
 5. Thread safety - focussing on the case where launched code-under-test starts a thread which makes mock calls.
    - Fixing any breakage to Google Mock locking, and applying sanitiser/fuzzer tools.


## Documentation
User documentation comes in three parts:
 1. [Getting Started with Cotest](coroutines/docs/getting-started.md)
 2. [Interworking with Google Mock](coroutines/docs/working-with-gmock.md)
 3. [Cotest Server Style](coroutines/docs/server-style.md)

For background on the idea of coroutine-based testing, see
 - [Discussion: Testing with Two Computers](coroutines/docs/testing-with-two-computers.md) for background on coroutine-based testing

## Examples

 - [Unit tests](coroutines/test/cotest-mutex.cc) for a code sample that (mis-)uses a mutex.
 - [Examples](coroutines/test/cotest-serverised.cc) of _serverised_ testing style.

There are many more cotest scripts in [`test/`](coroutines/test/)

## To build and run the tests

Cotest currently only supports CMake. From repo top level:
```
mkdir build
cd build
cmake -Dcoro_build_tests=ON -Dgtest_build_tests=ON -Dgtest_build_samples=ON -Dgmock_build_tests=ON -DCMAKE_BUILD_TYPE=Debug -DGOOGLETEST_VERSION=0 -DCOROUTINES_VERSION=0 ../
make
ctest
```
This will run the googletest tests as well as the cotest tests. Use `-R ^co` for just the cotest tests.

