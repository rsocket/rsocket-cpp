# reactivesocket-cpp

C++ implementation of [ReactiveSocket](https://reactivesocket.io)

NOTE: This is a work in progress. It is not feature complete.

 - Supports all operations except requestResponse
 - Supports interop with Java (TCP sockets only intersecting transport)

<a href='https://travis-ci.org/ReactiveSocket/reactivesocket-cpp/builds'><img src='https://travis-ci.org/ReactiveSocket/reactivesocket-cpp.svg?branch=master'></a>

# Dependencies

Install `folly`:

```
brew install folly
```

# Building and running tests

After installing dependencies as above, you can build and run tests with:

```
# inside root ./reactivesocket-cpp
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=DEBUG ../
make -j
./tests
```
