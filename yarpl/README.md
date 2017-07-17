# yarpl: Yet Another Reactive Programming Library

C++ implementation of reactive functional programming including both `Observable` and `Flowable` types.

NOTE: This is a work in progress. It is not feature complete.

# Build Status

no automated builds yet

# Building and running tests

After installing dependencies as above, you can build and run tests with:

```
# inside root ./yarpl
mkdir -p build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=DEBUG
make -j
./yarpl-tests
```

# Related Projects

- ReactiveX: http://reactivex.io
- RxJava 2.0: https://github.com/ReactiveX/RxJava/blob/2.x/DESIGN.md
- RxCpp: https://github.com/Reactive-Extensions/RxCpp
- Reactive Streams: http://www.reactive-streams.org
- Reactive Streams C++: https://github.com/ReactiveSocket/reactive-streams-cpp

# Why?

- Need an implementation of Reactive Streams `Publisher` (the `Flowable` type in this library) which does not exist anywhere that I'm aware of in C++
- Prefer having `Flowable`, `Observable`, `Single`, and `Completable` in a single interoperable library.

# What's with the name?

I don't enjoy naming project. Please suggest a better name. Until then I just want to code.

Also ...

- RxCpp already exists.
- RsCpp (ReactiveStreamsCpp) doesn't make sense as this isn't just about Reactive Streams, and the point of Reactive Streams is to have many implementations.
- I'm tired of projects starting with or containing "react", "reactive", "reactor", "rx", etc.
