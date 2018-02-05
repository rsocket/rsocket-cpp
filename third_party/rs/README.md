# `rs`

*rs* is an unofficial [Reactive Streams](http://www.reactive-streams.org/) library that offers types for reactive streams along with functions that operate on them.

## Features

* It is written in C++14.
* Like all Reactive Streams based libraries, it offers mandatory non-blocking back-pressure support (at the time of writing, [RxCpp](https://github.com/Reactive-Extensions/RxCpp) does not do this).
* It is extensible: Users of the library can create their own stream operators that look and feel exactly like the built-in operators. There is no built-in class that has a member function for each operator that exists like in [RxJava](https://github.com/ReactiveX/RxJava) or RxCpp.
* It has a trivial threading model: Nothing in this library is thread safe.
* It attempts to be small, to avoid excessive compile times.
* It does not (yet?) have the concept of a Scheduler, or time, like Rx libraries do.
* It comes with [a good test suite](test).


## In depth documentation

* [Reference documentation](doc/reference.md): Documentation for each of the operators, helpers and types in rs.
* [Reactive Streams specification](doc/specification.md): This document specifies the raw Reactive Streams concepts in C++. It is useful in order to get an in depth understanding of the rs library and required reading in order to be able to write custom Publishers and custom operators on Publishers.
* [Guide to writing custom operators](doc/custom_operators.md)


## Introduction

rs tries not to be an innovative library. It steals most of its ideas and names from Reactive Streams and [ReactiveX](http://reactivex.io/). It is conceptually very similar to ReactiveX libraries, for example RxJava. A lot of information about RxJava applies directly to rs. If you are unsure about what the underlying idea of the rs library is, it might help to read tutorials or watch presentations on Reactive Streams and ReactiveX.

The main entity of the rs library is the *Publisher*. Similar to a future or a promise, a Publisher represents an asynchronous computation. An idiomatic use of the rs libray is to make procedures that perform asynchronous operations return a Publisher, for example:

```cpp
AnyPublisher<User> LookupUserById(const std::string &user_id);
AnyPublisher<std::string> UsernameToUserId(const std::string &username);
```

People who are familiar with futures or promises will recognize this pattern. In many ways a Publisher is used just like a future object.

`LookupUserById` and `UsernameToUserId` above are asynchronous procedures that return immediately. The returned Publisher objects can be used to subscribe to and transform the results.


### Creating Publishers

The rs library has helper functions to create Publisher objects. Here are some of them:

* `Empty()` returns a Publisher that emits no values when subscribed to.
* `Just(args...)` returns a Publisher that emits just the given values. `Just()` is equivalent to `Empty()` and `Just(1, 2)` emits 1 and 2.
* `From(container)` returns a Publisher that emits all the values in the provided STL-style container, for example an `std::vector`.
* `Range(start, count)` returns a Publisher that counts upwards from `start`. For example, `Range(2, 3)` emits 2, 3 and 4.

These are often handy but none of them are asynchronous sources. This is because rs in itself does not do anything asynchronous. In practice, rs will often be used together with a library that offers asynchronous Publisher sources. For example, the *rs-grpc* library provides Publishers that asynchronously emit the responses of gRPC calls.

It is also possible but not very common in application code to create custom Publishers from scratch. The [rs specification](doc/specification.md#1-publisher-code) provides detailed information about the exact requirements of custom Publisher types.


### Manipulating Publishers

rs has a rich library of operators on Publisher objects (functions that take a Publisher and return a Publisher that behaves differently in some way). For example, the `Map` function takes a mapper function and returns an operator that modifies each element in a stream, much like the functional programming map function works for lists:

```cpp
// reverse_username is a functor that takes a Publisher of User objects and
// returns a Publisher of User objects.
auto reverse_username = Map([](User &&user) {
  auto username = user.username();
  std::reverse(username.begin(), username.end());
  user.set_username(username);
  return user;
});

AnyPublisher<User> user = LookupUserById("123");

// user_with_reversed_username is a Publisher that emits User
auto user_with_reversed_username = reverse_username(user);
```

Because it is very common to plumb several Publisher operators together, rs has a `Pipe` helper function:

```cpp
// This does the same as the code above
auto user_with_reversed_username = Pipe(
    LookupUserById("123"),
    Map([](User &&user) {
      auto username = user.username();
      std::reverse(username.begin(), username.end());
      user.set_username(username);
      return user;
    }));
```

`Pipe(a, b, c)` is basically the same as writing `c(b(a))`.

Another useful operator is `ConcatMap`, which is similar to `Map` but it allows the mapper function to return a Publisher. It is useful for chaining asynchronous operations:

```cpp
auto user = Pipe(
    UsernameToUserId("john_doe"),
    ConcatMap([](const std::string &user_id) {
      return LookupUserById(user_id);
    }));
```

Or, more concisely:

```cpp
auto user = Pipe(
    UsernameToUserId("john_doe"),
    ConcatMap(&LookupUserById));
```

One of the best features of the rs library is that operators compose very well:

```cpp
// sum is a Publisher that emits one value: The sum of squares of all even
// numbers between 1 and 100.
auto sum = Pipe(
    Range(1, 100),
    Filter([](int x) { return (x % 2) == 0; }),
    Map([](int x) { return x * x; }),
    Sum());
```


### Subscribing to Publishers

Once you have created a Publisher and applied the necessary operations on it, you will most likely want to access the values of the stream.

Most applications do not directly subscribe to Publishers. Instead, they let some library or framework do that for them. In rs-grpc, for example, RPC handler functions return a Publisher, which the rs-grpc library subscribes to.

It is also possible to subscribe directly to Publishers. Here is an example of doing that:

```cpp
auto subscription = int_publisher.Subscribe(MakeSubscriber(
    [](int value) {
      printf("Got value: %d\n", value);
    },
    [](std::exception_ptr &&error) {
      printf("Something went wrong\n");
    },
    [] {
      printf("The stream completed successfully\n");
    }));
// It is possible to request only some elements from the stream at a time. This
// can be useful if there is a risk that the publisher could produce data
// faster than the subscriber can handle it.
//
// No elements are provided to the Subscriber until they are requested. Here,
// an unbounded number of elements are requested, which effectively gives the
// Publisher a green light to push elements as quickly as it can.
subscription.Request(ElementCount::Unbounded());
```


### The (absence of a) `Publisher` type

In the Java version of Reactive Streams [`Publisher`](https://github.com/reactive-streams/reactive-streams-jvm/blob/master/api/src/main/java/org/reactivestreams/Publisher.java), [`Subscriber`](https://github.com/reactive-streams/reactive-streams-jvm/blob/master/api/src/main/java/org/reactivestreams/Subscriber.java) and [`Subscription`](https://github.com/reactive-streams/reactive-streams-jvm/blob/master/api/src/main/java/org/reactivestreams/Subscription.java) are Java interfaces. Looking at those interfaces, one might expect to find pure virtual `Publisher`, `Subscriber` and `Subscription` classes in rs, but there are no such classes.

Instead, in rs, Publisher, Subscriber and Subscription are *concepts*, much like iterators in C++. An rs Publisher is any C++ object of a type that fulfills [the requirements of the Publisher concept](doc/specification.md#1-publisher-code).

This can be a bit counterintuitive at first. For example, the return type of `Just(1)` is not `AnyPublisher<int>`; it is a type that doesn't even have a name (because the type contains the type of a lambda expression). The public API of rs does not give a name for the type of `Just(1)`, it only promises that it conforms to the Publisher concept.

This design allows rs to be very efficient when chaining operators: The compiler can see and inline chains of operators as it sees fit, since there aren't virtual method calls that hide things.


### Eraser types

Although the concept-based (as opposed to pure virtual class-based) design of rs can offer great performance and flexibility, it can sometimes force code to expose more type information to its callers than desired. As an example, let's look at a function `EvenSquares(int n)` that that computes the sum of squares of even numbers between 1 and `n`:

```cpp
auto EvenSquares(int n) {
  return Pipe(
      Range(1, n),
      Filter([](int x) { return (x % 2) == 0; }),
      Map([](int x) { return x * x; }),
      Sum());
}
```

In this code, the return type of `EvenSquares` doesn't have a name. In fact, the easiest way to give a name to it would probably be to write `decltype(... the function body goes here ...))`. If this was to be exposed as a library function the implementation would have to be in the header file, which is not nice.

To solve this type of problem, rs has *eraser types* for the Publisher, Subscriber and Subscription concepts:

```cpp
AnyPublisher<int> EvenSquares(int n) {
  return AnyPublisher<int>(Pipe(
      Range(1, n),
      Filter([](int x) { return (x % 2) == 0; }),
      Map([](int x) { return x * x; }),
      Sum()));
}
```

Unlike the first version of `EvenSquares`, this one can easily live in a `.cpp` implementation file.

`AnyPublisher<int>` is a class that can be constructed with any Publisher of `int`s and behaves just like the one it was created with. The only difference is that it "hides" the type of the underlying Publisher (using virtual method calls).

In addition to `AnyPublisher`, there are also type erasers for Subscribers and Subscriptions.

It is possible to construct Publishers that can emit more than one type (and correspondingly Subscribers that can receive more than one type). They can also be type erased, for example with `AnyPublisher<int, std::string>`. Publishers that never emit a value and only ever complete or fail can be type erased with `AnyPublisher<>`.


### Concept predicates

There are type predicates that can be used to check if a type claims to conform to a specific concept:

* `IsPublisher<T>` is a `constexpr bool` that is true if `T` is a Publisher.
* `IsSubscriber<T>` is a `constexpr bool` that is true if `T` is a Subscriber.
* `IsSubscription<T>` is a `constexpr bool` that is true if `T` is a Subscription.




## Threading model

Unlike most other Rx libraries (and future/promise libraries too, for that matter), `rs` does not do anything at all about threads or concurrency. The only thread safety guarantee that the library provides is that separate objects can be used concurrently on separate threads (because it has no global mutable state).

The author of this library sees this not as a limitation but as a feature:

* There are precisely 0 thread safety bugs in this library, guaranteed.
* There is never any overhead from locking or atomic primitives that you don't need.
* This library never spawns threads or does anything behind your back, ever.
* The user is in full control of the threading model.

In order to use this library effectively, the application needs one or more runloops. If there are separate runloops on separate threads, then each stream should be confined to one given runloop.


