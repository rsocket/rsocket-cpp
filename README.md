# reactivesocket-cpp

C++ implementation of ReactiveSocket

NOTE: This is a work in progress. It is not feature complete, and does not yet comply with the protocol, so can not yet interop with any other implementation. 

 - Supports Channels, Subscription and Stream
 - Works C++ <-> C++ but there are some TODO items that will stop Java interop
 - supports HTTP/2 DuplexConnection 

# Build Status

<a href='https://travis-ci.org/ReactiveSocket/reactivesocket-cpp/builds'><img src='https://travis-ci.org/ReactiveSocket/reactivesocket-cpp.svg?branch=master'></a>

# Dependencies

Install `folly`:

```
brew install folly
```

After first checkout, initialize and update submodules:

```
# inside root ./reactivesocket-cpp
git submodule init
git submodule update --recursive
```

# Building and running tests

After installing dependencies as above, you can build and run tests with:

```
# inside root ./reactivesocket-cpp
mkdir -p build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=DEBUG
make -j
./tests
```
