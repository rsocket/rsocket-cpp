# `rs` reference documentation

This document has API documentation for the rs Reactive Streams library.


## Table of Contents

<!--

To regenerate, run this Javascript in the browser console in the Github render of this markdown:

Array.from(document.getElementsByTagName('article')[0].getElementsByTagName('h2'))
    .slice(1)  // Skip the ToC
    .map(function(header) {
      var name = header.innerText;
      var link = header.getElementsByTagName('a')[0].href.replace(/.*#/, '#');
      return '* [`' + name + '`](' + link + ')';
    })
    .join('\n');

-->

* [`All(Predicate)`](#allpredicate)
* [`AnyPublisher`](#anypublisher)
* [`AnySubscriber`](#anysubscriber)
* [`AnySubscription`](#anysubscription)
* [`Append(Publisher...)`](#appendpublisher)
* [`Average()`](#average)
* [`BuildPipe(Operator...)`](#buildpipeoperator)
* [`Catch(Publisher)`](#catchpublisher)
* [`Concat(Publisher...)`](#concatpublisher)
* [`ConcatMap(Mapper)`](#concatmapmapper)
* [`Contains(Value)`](#containsvalue)
* [`Count(Publisher...)`](#countpublisher)
* [`DefaultIfEmpty(Value...)`](#defaultifemptyvalue)
* [`ElementAt(size_t)`](#elementatsize_t)
* [`ElementCount`](#elementcount)
* [`Empty()`](#empty)
* [`EndWith(Value...)`](#endwithvalue)
* [`EndWithGet(MakeValue...)`](#endwithgetmakevalue)
* [`Filter(Predicate)`](#filterpredicate)
* [`First()`](#first)
* [`First(Predicate)`](#firstpredicate)
* [`From(Container)`](#fromcontainer)
* [`IfEmpty(Publisher)`](#ifemptypublisher)
* [`IsPublisher`](#ispublisher)
* [`IsRvalue`](#isrvalue)
* [`IsSubscriber`](#issubscriber)
* [`IsSubscription`](#issubscription)
* [`Just(Value...)`](#justvalue)
* [`Last()`](#last)
* [`MakePublisher(Callback)`](#makepublishercallback)
* [`MakeSubscriber()`](#makesubscriber)
* [`MakeSubscriber(OnNext, OnError, OnComplete)`](#makesubscriberonnext-onerror-oncomplete)
* [`MakeSubscription()`](#makesubscription)
* [`MakeSubscription(RequestCb, CancelCb)`](#makesubscriptionrequestcb-cancelcb)
* [`Map(Mapper)`](#mapmapper)
* [`Max(Compare?)`](#maxcompare)
* [`Merge(Publisher...)`](#mergepublisher)
* [`Min(Compare?)`](#mincompare)
* [`Never()`](#never)
* [`Pipe(Publisher, Operator...)`](#pipepublisher-operator)
* [`Prepend(Publisher...)`](#prependpublisher)
* [`Publisher`](#publisher)
* [`PureVirtualSubscription`](#purevirtualsubscription)
* [`Range(Value, size_t)`](#rangevalue-size_t)
* [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer)
* [`ReduceGet(MakeInitial, Reducer)`](#reducegetmakeinitial-reducer)
* [`ReduceWithoutInitial<Accumulator>(Reducer)`](#reducewithoutinitialaccumulatorreducer)
* [`Repeat(Value, size_t)`](#repeatvalue-size_t)
* [`RequireRvalue`](#requirervalue)
* [`Scan(Accumulator, Mapper)`](#scanaccumulator-mapper)
* [`Skip(size_t)`](#skipsize_t)
* [`SkipWhile(Predicate)`](#skipwhilepredicate)
* [`Some(Predicate)`](#somepredicate)
* [`Splat(Functor)`](#splatfunctor)
* [`Start(CreateValue...)`](#startcreatevalue)
* [`StartWith(Value...)`](#startwithvalue)
* [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)
* [`Subscriber`](#subscriber)
* [`Subscription`](#subscription)
* [`Sum()`](#sum)
* [`Take(Count)`](#takecount)
* [`TakeWhile(Predicate)`](#takewhilepredicate)
* [`Throw(Exception)`](#throwexception)
* [`Throw(const std::exception_ptr &)`](#throwconst-stdexception_ptr-)
* [`VirtualSubscription`](#virtualsubscription)
* [`Zip(Publisher...)`](#zippublisher)
* [`Legend`](#legend)


## `All(Predicate)`

**Defined in:** [`rs/all.h`](../include/rs/all.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[bool])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#every), [ReactiveX](http://reactivex.io/documentation/operators/all.html)

**Description:** Operator that returns a stream that emits exactly one value: `true` if all the input elements match the given predicate, `false` otherwise. As soon as an element is encountered that matches the predicate, the boolean is emitted and the input stream is cancelled.

**Example usage:**

```cpp
auto input = Just(1, 2, 5, 1, -3, 1);
auto all_positive = Pipe(
    input,
    All([](int value) { return value > 0; }));
```

**See also:** [`Some(Predicate)`](#somepredicate)


## `AnyPublisher`

**Defined in:** [`rs/publisher.h`](../include/rs/publisher.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** As [described in the README](../README.md#the-absence-of-a-publisher-type), an rs Publisher is a C++ *concept* – like iterators in C++ – rather than a single concrete class. This permits aggressive compiler optimizations and some interesting API convenience features. However, it is sometimes useful to be able to give a name to any Publisher.

`AnyPublisher` is an [*eraser type*](../README.md#eraser-types) that uses virtual method calls to be able to give a name to the Publisher type.

`AnyPublisher` is a variadric template:

* `AnyPublisher<>` can encapsulate any Publisher that emits no values (it can only finish or fail).
* `AnyPublisher<T>` can encapsulate any Publisher that emits values of type `T`.
* `AnyPublisher<T, U>` can encapsulate any Publisher that emits values of type `T` and `U`.
* and so on.

**Example usage:**

In the example below, `AnyPublisher<int>` is used to be able to name the return type of `EvenSquares`, which makes it possible to place its definition in a `.cpp` implementation file, which would otherwise have been tricky:

```cpp
AnyPublisher<int> EvenSquares(int n) {
  return AnyPublisher<int>(Pipe(
      Range(1, n),
      Filter([](int x) { return (x % 2) == 0; }),
      Map([](int x) { return x * x; }),
      Sum()));
}
```

In the example below, `AnyPublisher<int>` is used to get one type for two different Publisher types. Without `AnyPublisher<int>` it would have been tricky to write the lambda in the example since it has to have only one return type:

```cpp
auto only_even = Pipe(
    Just(1, 2, 3, 4, 5),
    ConcatMap([](int x) {
      if ((x % 2) == 0) {
        return AnyPublisher<int>(Just(x));
      } else {
        return AnyPublisher<int>(Empty());
      }
    }));
```

**See also:** [`AnySubscriber`](#anysubscriber), [`AnySubscription`](#anysubscription)


## `AnySubscriber`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** As [described in the README](../README.md#the-absence-of-a-publisher-type), an rs Subscriber is a C++ *concept* – like iterators in C++ – rather than a single concrete class. This permits aggressive compiler optimizations and some interesting API convenience features. However, it is sometimes useful to be able to give a name to any Subscriber.

`AnySubscriber` is an [*eraser type*](../README.md#eraser-types) that uses virtual method calls to be able to give a name to the Subscriber type.

`AnySubscriber` is a variadric template:

* `AnySubscriber<>` can encapsulate any Subscriber that can't receive any values (it can only finish or fail).
* `AnySubscriber<T>` can encapsulate any Subscriber that can receive values of type `T`.
* `AnySubscriber<T, U>` can encapsulate any Subscriber that can receive values of type `T` and `U`.
* and so on.

The `AnySubscriber` eraser type is intended for advanced use of the rs library. Storing and passing around `AnySubscriber` objects is not done much except in custom operators, and most custom operators do not type erase Subscribers.

The `AnySubscriber` eraser type is used internally in the [`AnyPublisher`](#anypublisher) type eraser implementation.

**Example usage:**

```cpp
AnySubscriber<int> subscriber = AnySubscriber<int>(MakeSubscriber(
    [](int value) {
      printf("Got value: %d\n", value);
    },
    [](std::exception_ptr &&error) {
      printf("Something went wrong\n");
    },
    [] {
      printf("The stream completed successfully\n");
    }));

subscriber.OnNext(42);
subscriber.OnComplete();
```

**See also:** [`AnyPublisher`](#anypublisher), [`AnySubscription`](#anysubscription)


## `AnySubscription`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** As [described in the README](../README.md#the-absence-of-a-publisher-type), an rs Subscription is a C++ *concept* – like iterators in C++ – rather than a single concrete class. This permits aggressive compiler optimizations and some interesting API convenience features. However, it is sometimes useful to be able to give a name to any Subscription.

`AnySubscription` is an [*eraser type*](../README.md#eraser-types) that uses virtual method calls to be able to have one concrete type that can wrap any Subscription.

The `AnySubscription` eraser type is intended for advanced use of the rs library. It can be useful for classes that need to store Subscription objects as member variables: Type erasing the Subscription allows the header file with the class declaration to not leak information about exactly what is subscribed to.

**Example usage:**

```cpp
AnySubscription sub = AnySubscription(MakeSubscription(
    [](ElementCount count) {
      printf("%lld values requested\n", count.Get());
    },
    []() {
      printf("The Subscription was cancelled\n");
    }));
```

**See also:** [`AnyPublisher`](#anypublisher), [`AnySubscriber`](#anysubscriber)


## `Append(Publisher...)`

**Defined in:** [`rs/append.h`](../include/rs/append.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `Publisher[b], Publisher[c], ... -> (Publisher[a] -> Publisher[a, b, c, ...])`

**Description:** Appends the provided streams to the end of a stream. `Append` is similar to [`Concat`](#concatpublisher) but it is more ergonomic to use together with [`Pipe`](#pipepublisher-operator).

**Example usage:**

```cpp
// stream is a stream that emits 1, 2, 3, "hello", "world"
auto stream = Pipe(
    Just(1, 2, 3),
    Append(Just("hello"), Just("world")));
```

```cpp
// It is possible to call Append with no parameters. Then it is a no-op
// operator.
//
// stream is a stream that emits 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    Append());
```

**See also:** [`Concat(Publisher...)`](#concatpublisher), [`EndWith(Value...)`](#endwithvalue), [`EndWithGet(MakeValue...)`](#endwithgetmakevalue), [`Prepend(Publisher...)`](#prependpublisher), [`StartWith(Value...)`](#startwithvalue), [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)


## `Average()`

**Defined in:** [`rs/average.h`](../include/rs/average.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `void -> (Publisher[a] -> Publisher[ResultType])`, where `ResultType` is `double` by default and configurable by a template parameter to `Average()`.

**External documentation:** [RxMarbles](http://rxmarbles.com/#average), [ReactiveX](http://reactivex.io/documentation/operators/average.html)

**Description:** Operator that returns a stream that emits exactly one value: the (numeric) average of all of the input elements.

**Example usage:**

```cpp
auto average = Pipe(
    Just(1, 2, 3, 4, 5),
    Average());
```

## `BuildPipe(Operator...)`

**Defined in:** [`rs/pipe.h`](../include/rs/pipe.h)

**Kind:** [Operator Builder Builder](#kind_operator_builder_builder)

**[Type](#types):** `(Publisher[a] -> Publisher[b]), (Publisher[b] -> Publisher[c]), ... -> (Publisher[a] -> Publisher[c])`

**Description:** Helper function that can be used to combine a series of operators into a composite operator.

* `BuildPipe()` returns a unary identity function.
* `BuildPipe(x)` is basically equivalent to `x` if `x` is a unary functor.
* `BuildPipe(x, y)` is basically equivalent to `[](auto &&a) { return y(x(std::forward<decltype(a)>(a))); }`

**Example usage:**

```cpp
// root_mean_square is an operator that takes a Publisher and returns a
// Publisher that emits the root mean square of all the elements in the input
// publisher.
auto root_mean_square = BuildPipe(
    Map([](auto x) { return x * x; }),
    Average(),
    Map([](auto x) { return sqrt(x); }));

// Returns a Publisher that emits sqrt((1*1 + 2*2 + 3*3) / 3)
root_mean_square(Just(1, 2, 3))
```

**See also:** [`Pipe(Publisher, Operator...)`](#pipepublisher-operator)


## `Catch(Publisher)`

**Defined in:** [`rs/catch.h`](../include/rs/catch.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(std::exception_ptr&& -> Publisher[b]) -> (Publisher[a] -> Publisher[a, b])`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/catch.html)

**Description:** `Catch` is rs' asynchronous version of the `try`/`catch` statement in C++. It creates and operator that takes a Publisher and returns a Publisher that if successful behaves the same, but it if fails invokes the provided callback with the `std::exception_ptr` error and concatenates the callback's return value to the input stream.

**Example usage:**

```cpp
// one_to_ten is a Publisher that enumerates integers from 1 to 10
auto one_to_ten = Pipe(
    Range(1, 10),
    Catch([](std::exception_ptr &&error) {
      printf("This is not called because the stream does not fail\n");
      return Empty();
    }));
```

```cpp
auto video_id = Pipe(
    video_content.Upload("filename.mp4"),
    Catch([](std::exception_ptr &&error) {
      // If the Upload operation fails, this is called.

      // Log the error and rethrow
      printf("Video upload failed\n");
      return Throw(error);
    }));
```

```cpp
auto video_id = Pipe(
    video_content.Upload("filename.mp4"),
    Catch([](std::exception_ptr &&error) {
      // If the Upload operation fails, this is called.

      // Handle the error
      return Just(video_content.MakeEmptyId());
    }));
```


## `Concat(Publisher...)`

**Defined in:** [`rs/concat.h`](../include/rs/concat.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `Publisher[a], Publisher[b], ... -> Publisher[a, b, ...]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#concat), [ReactiveX](http://reactivex.io/documentation/operators/concat.html)

**Description:** Concatenate zero or more Publishers. The values of the first stream will be emitted first. Then, the values of the second stream are emitted and so on.

**Example usage:**

```cpp
auto count_to_100_twice = Concat(
    Range(1, 100),
    Range(1, 100));
```

```cpp
// Concat with only one stream creates a Publisher that behaves just like the
// inner publisher.
auto one_two_three_stream = Concat(Just(1, 2, 3));
```

```cpp
// Concat() is equivalent to Empty()
auto empty = Concat();
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Empty()`](#empty), [`Prepend(Publisher...)`](#prependpublisher)


## `ConcatMap(Mapper)`

**Defined in:** [`rs/concat_map.h`](../include/rs/concat_map.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> Publisher[b]) -> (Publisher[a] -> Publisher[b])`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/flatmap.html)

**Description:** Similar to [`Map(Mapper)`](#mapmapper), but `ConcatMap` takes a mapper function that returns a Publisher rather than a value. This makes it possible for the mapper function to perform asynchronous operations or to return zero or more than one value. This is similar to the [`flatMap` method in Java 8 Streams](https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html#flatMap-java.util.function.Function-).

**Example usage:**

```cpp
auto user_ids = std::vector<std::string>{ "1", "2", "3" };

// ConcatMap can be used when the mapper function needs to perform asynchronous
// operations
auto users = Pipe(
    From(user_ids),
    ConcatMap([user_database](const std::string &user_id) {
      return user_database->Lookup(user_id);
    }));
```

```cpp
// ConcatMap can be used to implement the Filter operator
auto only_even = Pipe(
    Just(1, 2, 3, 4, 5),
    ConcatMap([](int x) {
      if ((x % 2) == 0) {
        return Publisher<int>(Just(x));
      } else {
        return Publisher<int>(Empty());
      }
    }));
```

```cpp
// zero zeroes, one one, two twos, three threes etc. This stream emits:
// 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5
auto numbers = Pipe(
    Range(0, 5),
    ConcatMap([](int x) {
      return Repeat(x, x);
    }));
```

**See also:** [`Map(Mapper)`](#mapmapper), [`Concat(Publisher...)`](#concatpublisher)


## `Contains(Value)`

**Defined in:** [`rs/contains.h`](../include/rs/contains.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `Value, (Value, Value -> bool)? -> (Publisher[a] -> Publisher[bool])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#includes), [ReactiveX](http://reactivex.io/documentation/operators/contains.html)

**Description:** Operator that returns a stream that emits exactly one value: `true` if any of the input elements are equal to the given value to search for, `false` otherwise. As soon as an element is encountered that matches the predicate, the boolean is emitted and the input stream is cancelled.

It is possible to pass a custom equality operator function to `Contains`. The default is `std::equal_to`, which uses `operator==`.

**Example usage:**

```cpp
auto has_five = Pipe(
    Just(1, 2, 5, 1, -3, 1),
    Contains(5));
```


## `Count(Publisher...)`

**Defined in:** [`rs/count.h`](../include/rs/count.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `void -> (Publisher[a] -> Publisher[CountType])`, where `CountType` is `int` by default and configurable by a template parameter to `Count()`.

**External documentation:** [RxMarbles](http://rxmarbles.com/#count), [ReactiveX](http://reactivex.io/documentation/operators/count.html)

**Description:** Count the number of elements in a Publisher and emit only one value: The number of elements in the input stream.

**Example usage:**

```cpp
// count is a Publisher that emits just the int 3
auto count = Count(Just(1, 2, 3));
```

```cpp
// By default, Count counts using an int, but that is configurable
auto count = Count<double>(Just(1));
```

**See also:** [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), [`Sum()`](#sum)


## `DefaultIfEmpty(Value...)`

**Defined in:** [`rs/default_if_empty.h`](../include/rs/default_if_empty.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `a, b, ... -> (Publisher[c] -> Publisher[a, b, c, ...])`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/defaultifempty.html)

**Description:** `DefaultIfEmpty` takes a Publisher and returns one that behaves just like it, except that if it finishes without emitting any elements, it emits given values.

**Example usage:**

```cpp
// Because the input stream emits values, DefaultIfEmpty does nothing in this
// case.
auto one_two_three = Pipe(
    Just(1, 2, 3),
    DefaultIfEmpty(42));
```

```cpp
// The input stream is empty, so DefaultIfEmpty emits 42
auto fourty_two = Pipe(
    Empty(),
    DefaultIfEmpty(42));
```

```cpp
// DefaultIfEmpty accepts any number of parameters
auto one_two_three = Pipe(
    Empty(),
    DefaultIfEmpty(1, 2, 3));

auto empty = Pipe(
    Empty(),
    DefaultIfEmpty());  // DefaultIfEmpty with no parameters is a no-op.
```

**See also:** [`IfEmpty(Publisher)`](#ifemptypublisher)


## `ElementAt(size_t)`

**Defined in:** [`rs/element_at.h`](../include/rs/element_at.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `size_t -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#elementAt), [ReactiveX](http://reactivex.io/documentation/operators/elementat.html)

**Description:** Takes a stream and returns a stream that emits only one value: The `nth` element of the stream (zero indexed). If the number of elements in the stream is less than or equal to `nth`, the returned stream fails with an `std::out_of_range` exception.

**Example usage:**

```cpp
auto fifty = Pipe(
    Range(0, 100),
    ElementAt(50));
```

```cpp
// Fails with an std::out_of_range exception.
auto fail = Pipe(
    Just(1),
    ElementAt(1));
```

**See also:** [`First(Predicate)`](#firstpredicate)


## `ElementCount`

**Defined in:** [`rs/element_count.h`](../include/rs/element_count.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** `ElementCount` is the type that is given to the `Request` method on the Subscription concept. It is possible to `Request` zero or more values, and it's also possible to `Request` all the values of a stream by calling `Request(ElementCount::Unbounded())`.

In [the Java version of Reactive Streams](https://github.com/reactive-streams/reactive-streams-jvm#3.17), the `Subscription.request` method takes a `long` and uses `java.lang.Long.MAX_VALUE` to signify an unbounded number of elements. rs could have simply used C++'s corresponding `long long` type, but I found that doing so is very prone to integer overflow bugs.

`ElementCount` is similar to a `long long` but it handles integer overflow automatically.

**Example usage:**

```cpp
// Construct and assign ElementCount values
auto a = ElementCount(1);
auto unbounded = ElementCount::Unbounded();
a = ElementCount(2);
a = 3;

// Inspect the value
bool is_unbounded = unbounded.IsUnbounded();
long long value = a.Get();

// ElementCount overloads the basic addition and subtraction operators
auto b = a + a;
auto c = b - a;
auto d = a + 1;
auto e = a - 1;
b++;
++b;
b--;
--b;
b += a;
b -= a;
b += 1;
b -= 1;

// ElementCount can be compared with other values
bool f = a < b;
bool g = a < 1;
bool h = a <= b;
bool i = a <= 1;
bool j = a > b;
bool k = a > 1;
bool l = a >= b;
bool m = a >= 1;
bool n = a == b;
bool o = a != b;
bool p = a == 1;
bool q = 1 == a;

// Overflow handling:
unbounded == unbounded;
unbounded + 1 == unbounded;
unbounded - 1 == unbounded;
unbounded + unbounded == unbounded;
unbounded - unbounded == unbounded;
```


## `Empty()`

**Defined in:** [`rs/empty.h`](../include/rs/empty.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `void -> Publisher[]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/empty-never-throw.html)

**Description:** Creates a stream that emits no values.

**Example usage:**

```cpp
// empty is a Publisher that emits no values.
auto empty = Empty();
```

**See also:** [`Just(Value...)`](#justvalue), [`Never()`](#never)


## `EndWith(Value...)`

**Defined in:** [`rs/end_with.h`](../include/rs/end_with.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `a, b, ... -> (Publisher[c] -> Publisher[a, b, ..., c]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#startWith), [ReactiveX](http://reactivex.io/documentation/operators/startwith.html)

**Description:** Appends the provided values to a stream.

The parameters of `EndWith` must be copyable. If they are not, consider using [`EndWithGet(MakeValue...)`](#endwithgetmakevalue).

**Example usage:**

```cpp
// stream is a stream that emits 1, 2, 3, "hello", "world"
auto stream = Pipe(
    Just(1, 2, 3),
    EndWith("hello", "world"));
```

```cpp
// It is possible to call EndWith with no parameters. Then it is a no-op
// operator.
//
// stream is a stream that emits 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    EndWith());
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Concat(Publisher...)`](#concatpublisher), [`EndWithGet(MakeValue...)`](#endwithgetmakevalue), [`Prepend(Publisher...)`](#prependpublisher), [`StartWith(Value...)`](#startwithvalue), [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)


## `EndWithGet(MakeValue...)`

**Defined in:** [`rs/end_with.h`](../include/rs/end_with.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(void -> a), (void -> b), ... -> (Publisher[c] -> Publisher[a, b, ..., c]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#startWith), [ReactiveX](http://reactivex.io/documentation/operators/startwith.html)

**Description:** `EndWithGet` is like [`EndWith(Value...)`](#endwithvalue), but it takes functions that create the values to be emitted rather than the values directly. This can be used when the values are expensive to craete or when they are noncopyable.

**Example usage:**

```cpp
// stream is a stream that emits 1, 2, 3, "hello", "world"
auto stream = Pipe(
    Just(1, 2, 3),
    EndWithGet([] { return "hello"; }, [] { return "world"; }));
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Concat(Publisher...)`](#concatpublisher), [`EndWith(Value...)`](#endwithvalue), [`Prepend(Publisher...)`](#prependpublisher), [`StartWith(Value...)`](#startwithvalue), [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)


## `Filter(Predicate)`

**Defined in:** [`rs/filter.h`](../include/rs/filter.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#filter), [ReactiveX](http://reactivex.io/documentation/operators/filter.html)

**Description:** Takes a Publisher and returns one that behaves like it, except that it only emits the values that match a given predicate. It is similar to the `filter` function in functional programming (there are variations of it in [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/filter), [Java 8 Streams](https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html#filter-java.util.function.Predicate-) and [Boost Ranges](http://www.boost.org/doc/libs/1_64_0/libs/iterator/doc/filter_iterator.html), for example).

**Example usage:**

```cpp
auto divisible_by_three = Pipe(
    Range(1, 100000),
    Filter([](int x) { return (x % 3) == 0; }));
```

**See also:** [`Skip(size_t)`](#skipsize_t), [`SkipWhile(Predicate)`](#skipwhilepredicate)


## `First()`

**Defined in:** [`rs/first.h`](../include/rs/first.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `void -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#first), [ReactiveX](http://reactivex.io/documentation/operators/first.html)

**Description:** Takes a stream and returns a stream that emits only the first value in the input stream. If the input stream is empty, the returned stream fails with an `std::out_of_range` exception.

As soon as one element has been emitted from the input stream, the returned stream completes and the input stream is cancelled.

**Example usage:**

```cpp
auto five = Pipe(
    Just(5, 10, 15),
    First());
```

```cpp
// Fails with an std::out_of_range exception.
auto fail = Pipe(
    Empty(),
    First());
```

**See also:** [`ElementAt(size_t)`](#elementatsize_t), [`Last()`](#last), [`Take(Count)`](#takecount)


## `First(Predicate)`

**Defined in:** [`rs/first.h`](../include/rs/first.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#first), [ReactiveX](http://reactivex.io/documentation/operators/first.html)

**Description:** Takes a stream and returns a stream that emits only the first value in the input stream that matches the given predicate. If the input stream is empty or if no elements matched the predicate, the returned stream fails with an `std::out_of_range` exception.

As soon as one element that matches the predicate has been emitted from the input stream, the returned stream completes and the input stream is cancelled.

**Example usage:**

```cpp
auto ten = Pipe(
    Just(5, 10, 15),
    First([](int x) { return x > 7; }));
```

```cpp
// Fails with an std::out_of_range exception.
auto fail = Pipe(
    Just(1, 2, 3, 4),
    First([](int x) { return x > 7; }));
```

**See also:** [`ElementAt(size_t)`](#elementatsize_t), [`Take(Count)`](#takecount)


## `From(Container)`

**Defined in:** [`rs/from.h`](../include/rs/from.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `Container[a] -> Publisher[a]`, where `Container[a]` is an STL-style container, for example `std::vector<a>`.

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/from.html)

**Description:** Create a Publisher from an STL-style container.

**Example usage:**

```cpp
auto numbers = From(std::vector<int>{ 1, 2, 3, 4, 5 });
```

**See also:** [`Just(Value...)`](#justvalue), [`Empty()`](#empty)


## `IfEmpty(Publisher)`

**Defined in:** [`rs/if_empty.h`](../include/rs/if_empty.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `Publisher[a] -> (Publisher[b] -> Publisher[a, b])`

**Description:** `IfEmpty` takes a Publisher and returns one that behaves just like it, except that if it finishes without emitting any elements, it emits the values of the provided default stream. Similar to, but more generic, than [`DefaultIfEmpty(Value...)`](#defaultifemptyvalue).

**Example usage:**

```cpp
// Because the input stream emits values, IfEmpty does nothing in this case.
auto one_two_three = Pipe(
    Just(1, 2, 3),
    IfEmpty(Just(42)));
```

```cpp
// Here, IfEmpty is used to require that the input stream emits at least one
// element.
auto fail = Pipe(
    Empty(),
    IfEmpty(Throw(std::out_of_range("Input must not be empty"))));
```

**See also:** [`DefaultIfEmpty(Value...)`](#defaultifemptyvalue)


## `IsPublisher`

**Defined in:** [`rs/publisher.h`](../include/rs/publisher.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** `IsPublisher` is a type predicate that checks if a given type claims to conform to the Publisher concept. (It checks if the given type publically inherits [`Publisher`](#publisher).)

**Example usage:**

```cpp
template <typename PublisherT>
void TakePublisher(const PublisherT &publisher) {
  static_assert(
      IsPublisher<PublisherT>,
      "TakePublisher must be called with a Publisher");

  // ...
}
```

**See also:** [`IsSubscriber`](#issubscriber), [`IsSubscription`](#issubscription)


## `IsRvalue`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** `IsRvalue` is a type predicate that checks if a given type is a value or a mutable rvalue. It can be used in template functions with a generic parameter to check that it did not receive an lvalue reference. This can be useful to catch type errors earlier than they otherwise would have been found, improving error messages.

**Example usage:**

The Subscriber concept defines that its `OnNext` method takes an rvalue reference. The code below enforces that the parameter to `OnNext` is not an lvalue reference (which might mean that the calling code expects to still own the passed object afterwards).

```cpp
class MySubscriber : public Subscriber {
 public:
  template <typename T>
  void OnNext(T &&t) {
    static_assert(
        IsRvalue<T>,
        "MySubscriber::OnNext was invoked with a non-rvalue parameter");

    // ...
  }

  void OnError(std::exception_ptr &&error) {}
  void OnComplete() {}
};
```


## `IsSubscriber`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** `IsSubscriber` is a type predicate that checks if a given type claims to conform to the Subscriber concept. (It checks if the given type publically inherits [`Subscriber`](#subscriber).)

**Example usage:**

```cpp
template <typename SubscriberT>
void TakeSubscriber(const SubscriberT &subscriber) {
  static_assert(
      IsSubscriber<SubscriberT>,
      "TakeSubscriber must be called with a Subscriber");

  // ...
}
```

**See also:** [`IsPublisher`](#ispublisher), [`IsSubscription`](#issubscription)


## `IsSubscription`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** `IsSubscription` is a type predicate that checks if a given type claims to conform to the Subscription concept. (It checks if the given type publically inherits [`Subscription`](#subscription).)

**Example usage:**

```cpp
template <typename SubscriptionT>
void TakeSubscription(const SubscriptionT &subscription) {
  static_assert(
      IsSubscription<SubscriptionT>,
      "TakeSubscription must be called with a Subscription");

  // ...
}
```

**See also:** [`IsPublisher`](#ispublisher), [`IsSubscription`](#issubscription)


## `Just(Value...)`

**Defined in:** [`rs/just.h`](../include/rs/just.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `a, b, ... -> Publisher[a, b, ...]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/just.html)

**Description:** Constructs a Publisher that emits the given value or values.

The provided values must be copyable. If they are not, [`Start(CreateValue...)`](#startcreatevalue) can be used instead.

**Example usage:**

```cpp
auto one = Just(1);

// It's possible to create a stream with more than one value
auto one_and_two = Just(1, 2);
// It's possible to create a stream with more zero values; this is like Empty
auto empty = Just();

// The types of the values to emit may be different
auto different_types = one_hi = Just(1, "hi");
```

**See also:** [`Empty()`](#empty), [`From(Container)`](#fromcontainer), [`Start(CreateValue...)`](#startcreatevalue)


## `Last()`

**Defined in:** [`rs/last.h`](../include/rs/last.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `void -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#last), [ReactiveX](http://reactivex.io/documentation/operators/last.html)

**Description:** Takes a stream and returns a stream that emits only the last value in the input stream. If the input stream is empty, the returned stream fails with an `std::out_of_range` exception.

**Example usage:**

```cpp
auto fifteen = Pipe(
    Just(5, 10, 15),
    Last());
```

```cpp
// Fails with an std::out_of_range exception.
auto fail = Pipe(
    Empty(),
    Last());
```

**See also:** [`ElementAt(size_t)`](#elementatsize_t), [`First()`](#first)


## `MakePublisher(Callback)`

**Defined in:** [`rs/publisher.h`](../include/rs/publisher.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `(Subscriber[a]&& -> Subscription[]) -> Publisher[a]`

**Description:** Helper function that can be used to create custom Publishers. Please note that creating a custom Publisher is considered an advanced use of the rs library. When doing so, you must ensure that your Publisher object conforms to the rules in the [rs specification](specification.md).

`MakePublisher` takes a functor, for example a lambda, that takes a Subscriber and returns a Subscription and uses that to create a Publisher. It is also possible, but sometimes more verbose, to create a custom Publisher type directly by defining a class that inherits [`Publisher`](#publisher) and that has a `Subscribe` method.

**Example usage:**

```cpp
// An implementation of the Empty operator:
auto Empty() {
  return MakePublisher([](auto &&subscriber) {
    subscriber.OnComplete();
    return MakeSubscription();
  });
}
```


## `MakeSubscriber()`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `void -> Subscriber[a]`

**Description:** Creates a Subscriber that doesn't do anything. This is useful mostly in test code.

**Example usage:**

```cpp
auto my_dummy_subscriber = MakeSubscriber();
my_dummy_subscriber.OnNext(1);  // Does nothing
my_dummy_subscriber.OnNext("test");  // Does nothing
my_dummy_subscriber.OnComplete();  // Does nothing
```


## `MakeSubscriber(OnNext, OnError, OnComplete)`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `(a -> void), (std::exception_ptr&& -> void), (void -> void) -> Subscriber[a]`

**Description:** Creates a Subscriber from a set of callback functors. It is always possible to create your own Subscriber class by inheriting [`Subscriber`](#subscriber) and defining the `OnNext`, `OnComplete` and `OnError` methods, but this helper function is often more convenient.

Most application code requires few or no custom Subscribers. The most common cases where it is necessary to create your own is to terminate a Publisher to get its final result or to create a custom operator that cannot be built by combining operators that already exist.

Regardless of whether this helper is used or if you create your own Subscriber class, you must follow the [rules for Subscribers as defined in the rs specification](specification.md#2-subscriber-code).

**Example usage:**

```cpp
auto fib = Just(1, 1, 2, 3, 5);
// Subscribe to the values of fib
auto subscription = fib.Subscribe(MakeSubscriber(
    [](int value) {
      printf("Got value: %d\n", value);
    },
    [](std::exception_ptr &&error) {
      printf("Something went wrong\n");
    },
    [] {
      printf("The stream completed successfully\n");
    }));
subscription.Request(ElementCount::Unbounded());
```


## `MakeSubscription()`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `void -> Subscription[]`

**Description:** Creates a Subscription that doesn't do anything. This is useful mostly in test code.

**Example usage:**

```cpp
auto my_dummy_subscription = MakeSubscription();
my_dummy_subscription.Request(ElementCount(1));  // Does nothing
my_dummy_subscription.Cancel();  // Does nothing
```


## `MakeSubscription(RequestCb, CancelCb)`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `(ElementCount -> void), (void -> void) -> Subscription[]`

**Description:** Creates a Subscription from a set of callback functors. It is always possible to create your own Subscription class by inheriting [`Subscription`](#subscription) and defining the `Request` and `Cancel` method, but this helper function is often more convenient.

Creating custom Subscription objects is usually needed only for making custom operators that cannot be built by combining operators that already exist.

Regardless of whether this helper is used or if you create your own Subscription class, you must follow the [rules for Subscriptions as defined in the rs specification](specification.md#3-subscription-code).

**Example usage:**

```cpp
auto sub = MakeSubscription(
    [](ElementCount count) {
      printf("%lld values requested\n", count.Get());
    },
    []() {
      printf("The Subscription was cancelled\n");
    });
```


## `Map(Mapper)`

**Defined in:** [`rs/map.h`](../include/rs/map.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> b) -> (Publisher[a] -> Publisher[b])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#map), [ReactiveX](http://reactivex.io/documentation/operators/map.html)

**Decription:** Takes a Publisher and returns one that emits the same number of values, but each value is transformed by the provided mapper function. It is similar to the `map` function in functional programming (there are variations of it in [Javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/map), [Java 8 Streams](https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html#map-java.util.function.Function-) and [Boost Ranges](http://www.boost.org/doc/libs/1_64_0/libs/iterator/doc/transform_iterator.html), for example).

**Example usage:**

```cpp
auto one_to_hundred_strings = Pipe(
    Range(1, 100),
    Map([](int num) {
      return std::to_string(num);
    }));
```

**See also:** [`ConcatMap(Mapper)`](#concatmapmapper), [`Scan(Accumulator, Mapper)`](#scanaccumulator-mapper)


## `Max(Compare?)`

**Defined in:** [`rs/max.h`](../include/rs/max.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a, a -> bool)? -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#max), [ReactiveX](http://reactivex.io/documentation/operators/max.html)

**Description:** Takes a stream and returns a stream that emits only the biggest value in the input stream. It is possible to pass a custom comparator function to `Max`. The default is `std::less`, which uses `operator<`.

**Example usage:**

```cpp
auto biggest = Pipe(
    Just(1, 5, 2, 6, 4, 2, 6, 2, 5, 8, 2, 1),
    Max());
```

**See also:** [`Min(Compare?)`](#mincompare), [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), [`ReduceWithoutInitial<Accumulator>(Reducer)`](#reducewithoutinitialaccumulatorreducer)


## `Merge(Publisher...)`

**Defined in:** [`rs/merge.h`](../include/rs/merge.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `Publisher[a], Publisher[b], ... -> Publisher[a, b, ...]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#merge), [ReactiveX](http://reactivex.io/documentation/operators/merge.html)

**Description:** Takes a number of streams and returns a stream that emits all of the values that the input streams emit, with no ordering between the streams (unlike [`Concat(Publisher...)`](#concatpublisher), which emits values from the streams one by one).

**Example usage:**

```cpp
auto six = Pipe(
    Merge(Just(2), Just(2), Just(2)),
    Sum());
```

**See also:** [`Concat(Publisher...)`](#concatpublisher), [`Zip(Publisher...)`](#zippublisher)


## `Min(Compare?)`

**Defined in:** [`rs/min.h`](../include/rs/min.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a, a -> bool)? -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#min), [ReactiveX](http://reactivex.io/documentation/operators/min.html)

**Description:** Takes a stream and returns a stream that emits only the smallest value in the input stream. It is possible to pass a custom comparator function to `Min`. The default is `std::less`, which uses `operator<`.

**Example usage:**

```cpp
auto smallest = Pipe(
    Just(1, 5, 2, 6, 4, 0, 6, 2, 5, 8, 2, 1),
    Min());
```

**See also:** [`Max(Compare?)`](#maxcompare), [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), [`ReduceWithoutInitial<Accumulator>(Reducer)`](#reducewithoutinitialaccumulatorreducer)


## `Never()`

**Defined in:** [`rs/never.h`](../include/rs/never.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `void -> Publisher[]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/empty-never-throw.html)

**Description:** Creates a Publisher that never emits any value and never finishes. Useful mostly for testing purposes.

**Example usage:**

```cpp
auto never = Never();
```

**See also:** [`Empty()`](#empty), [`Throw(Exception)`](#throwexception), [`Throw(const std::exception_ptr &)`](#throwconst-stdexception_ptr-), [`Just(Value...)`](#justvalue)


## `Pipe(Publisher, Operator...)`

**Defined in:** [`rs/pipe.h`](../include/rs/pipe.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `Publisher[a], (Publisher[a] -> Publisher[b]), (Publisher[b] -> Publisher[c]), ... -> Publisher[c]`

**Description:** When using rs, it is very common to combine multiple streams and operators, so it's important that doing so is concise and that the code is easy to read. `Pipe` is a helper function for that: `Pipe(a, b, c)` is basically the same as writing `c(b(a))` and `Pipe(a)` is basically the same as `a`

**Example usage:**

Without the `Pipe` operator, it is difficult to avoid writing tricky to read code such as:

```cpp
auto even = Filter([](int x) { return (x % 2) == 0; });
auto square = Map([](int x) { return x * x; });
auto sum = Sum();
auto sum_of_even_squares = sum(square(even(Range(1, 100))));
```

Many ReactiveX libraries such as RxCpp and RxJava make this convenient by making all operators methods on their `Publisher` type, to let the user write code along the lines of

```cpp
// This example does not actually work
auto sum_of_even_squares = Range(1, 100)
    .Filter([](int x) { return (x % 2) == 0; })
    .Map([](int x) { return x * x; })
    .Sum();
```

This code looks nice, but this design has the drawback of making it impossible for application code to add custom operators that are as convenient to use. rs avoids this by offering the `Pipe` helper function instead of a Publisher base class with all the built-in operators as methods:

```cpp
auto sum_of_even_squares = Pipe(
    Range(1, 100),
    Filter([](int x) { return (x % 2) == 0; }),
    Map([](int x) { return x * x; }),
    Sum());
```

This allows third-party code to add custom operators on Publishers that can be used exactly like the built-in operators. The rs built-ins receive no special treatment.

**See also:** [`BuildPipe(Operator...)`](#buildpipeoperator)


## `Prepend(Publisher...)`

**Defined in:** [`rs/prepend.h`](../include/rs/prepend.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `Publisher[b], Publisher[c], ... -> (Publisher[a] -> Publisher[a, b, c, ...])`

**Description:** Prepends the provided streams in the front of a stream. `Prepend` is similar to [`Concat`](#concatpublisher) but it is more ergonomic to use together with [`Pipe`](#pipepublisher-operator).

**Example usage:**

```cpp
// stream is a stream that emits "hello", "world", 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    Prepend(Just("hello"), Just("world")));
```

```cpp
// It is possible to call Prepend with no parameters. Then it is a no-op
// operator.
//
// stream is a stream that emits 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    Prepend());
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Concat(Publisher...)`](#concatpublisher), [`EndWith(Value...)`](#endwithvalue), [`EndWithGet(MakeValue...)`](#endwithgetmakevalue), [`StartWith(Value...)`](#startwithvalue), [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)


## `Publisher`

**Defined in:** [`rs/publisher.h`](../include/rs/publisher.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** One of the [requirements for Publisher types](specification.md#1-publisher-code) is that they must inherit `Publisher` to signify the intent of being a Publisher.

`Publisher` itself is a class that does not do anything at all; its sole purpose is to make it so that Publisher types have to declare that they want to be Publishers.

`Publisher` has a `protected` constructor. This makes it impossible to construct `Publisher` objects directly; only subclasses of it can be instantiated.

**See also:** [`Subscriber`](#subscriber), [`Subscription`](#subscription)


## `PureVirtualSubscription`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:**

`PureVirtualSubscription` is a pure virtual class version of the Subscription concept. It is only useful in some very specific use cases; this is not the main Subscription interface.

Any Subscription object can be turned into a PureVirtualSubscription with the
help of the [`VirtualSubscription`](#virtualsubscription) wrapper class.

```cpp
class PureVirtualSubscription : public Subscription {
 public:
  virtual ~PureVirtualSubscription();
  virtual void Request(ElementCount count) = 0;
  virtual void Cancel() = 0;
};
```

**See also:** [`AnySubscription`](#anysubscription), [`Subscription`](#subscription), [`VirtualSubscription`](#virtualsubscription)


## `Range(Value, size_t)`

**Defined in:** [`rs/range.h`](../include/rs/range.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `a, size_t -> Publisher[Numberic]`, where `a` is any numeric type.

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/range.html)

**Description:** Constructs a Publisher that emits a range of values. The first parameter is the first value of the range, the second parameter is the number of values to emit.

**Example usage:**

```cpp
auto ten_to_twenty = Range(10, 10);
```

**See also:** [`From(Container)`](#fromcontainer), [`Just(Value...)`](#justvalue), [`Repeat(Value, size_t)`](#repeatvalue-size_t)


## `Reduce(Accumulator, Reducer)`

**Defined in:** [`rs/reduce.h`](../include/rs/reduce.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `a, (a, b -> a) -> (Publisher[b] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#reduce), [ReactiveX](http://reactivex.io/documentation/operators/reduce.html)

**Description:** Reduces (this is called left fold in some languages and libraries) a stream of values into one single value. This is similar to `std::accumulate` and [the `reduce` method in Java 8 Streams](https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html#reduce-T-java.util.function.BinaryOperator-).

`Reduce` takes an initial accumulator value and a function that takes the accumulator and a value from the stream and combines them into one value. It works with empty streams.

`Reduce` works only with accumulator types that are copyable. For accumulator types that are noncopyable, use [`ReduceGet(MakeInitial, Reducer)`](#reducegetmakeinitial-reducer).

**Example usage:**

```cpp
// An implementation of the Sum operator that works with ints
auto Sum() {
  return Reduce(0, [](int accum, int value) {
    return accum + value;
  });
}
```

**See also:** [`ReduceGet(MakeInitial, Reducer)`](#reducegetmakeinitial-reducer), [`ReduceWithoutInitial<Accumulator>(Reducer)`](#reducewithoutinitialaccumulatorreducer)


## `ReduceGet(MakeInitial, Reducer)`

**Defined in:** [`rs/reduce.h`](../include/rs/reduce.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(void -> a), (a, b -> a) -> (Publisher[b] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#reduce), [ReactiveX](http://reactivex.io/documentation/operators/reduce.html)

**Description:** `ReduceGet` is like [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), but it takes a function that creates the initial value rather than the initial value directly. This can be used when the initial value is expensive to create or when it is not copyable.

`ReduceGet` works with empty streams.

**Example usage:**

```cpp
// An implementation of the Sum operator that works with ints
auto Sum() {
  return ReduceGet([] { return 0; }, [](int accum, int value) {
    return accum + value;
  });
}
```

**See also:** [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), [`ReduceWithoutInitial<Accumulator>(Reducer)`](#reducewithoutinitialaccumulatorreducer)


## `ReduceWithoutInitial<Accumulator>(Reducer)`

**Defined in:** [`rs/reduce.h`](../include/rs/reduce.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a, a -> a) -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#reduce), [ReactiveX](http://reactivex.io/documentation/operators/reduce.html)

**Description:** `ReduceWithoutInitial` is like [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer) except that instead of taking an initial accumulator value up-front it uses first value of the input stream.

`ReduceWithoutInitial` does not accept empty input streams; if the input stream finishes without emitting a value, the output stream fails with an `std::out_of_range` exception.

[`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer) and [`ReduceGet(MakeInitial, Reducer)`](#reducegetmakeinitial-reducer) allow the accumulator type to be different from the types of the input values, but `ReduceWithoutInitial` requires the accumulator to be of the same type as the input values.

**Example usage:**

```cpp
// An simplified implementation of the Max operator that works with ints
auto Max() {
  return ReduceWithoutInitial<int>([](int accum, int value) {
    return std::max(accum, value);
  });
}
```

**See also:** [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer), [`ReduceGet(MakeInitial, Reducer)`](#reducegetmakeinitial-reducer)


## `Repeat(Value, size_t)`

**Defined in:** [`rs/repeat.h`](../include/rs/repeat.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `a, size_t -> Publisher[a]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/repeat.html)

**Description:** Construct a Publisher that emits a specific value a given number of times.

**Example usage:**

```cpp
auto hello_a_hundred_times = Repeat("hello", 100);
```

**See also:** [`From(Container)`](#fromcontainer), [`Just(Value...)`](#justvalue), [`Range(Value, size_t)`](#rangevalue-size_t)


## `RequireRvalue`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Background:** This is a short introduction to universal references in C++. If you already know this, you can skip this background part.

When defining C++ function or method templates where a parameter type is inferred, the function will accept a value, an rvalue reference and an lvalue reference:

```cpp
template <typename T>
void UseValue(T &&t) {
  // ...
}

void UseString(std::string &&s) {
  // ...
}
```

In the example above, `T &&` is a [universal reference](https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers). This means that `UseValue` can be invoked with not just rvalue references but also lvalue references:

```cpp
UseString(std::string("hello"));  // Valid
UseValue(std::string("hello"));  // Valid

std::string a("a");
UseString(std::move(a));  // Valid
UseValue(std::move(a));  // Valid

std::string b("b");
UseString(b);  // NOT valid
UseValue(b);  // Also valid
```

The fact that `UseValue(b)` is valid while `UseString(b)` is not is a bit counterintuitive, but it is also quite useful because it allows [perfect forwarding](http://eli.thegreenplace.net/2014/perfect-forwarding-and-universal-references-in-c/).

**Description:** There are cases where it is useful to have functions or methods that take an rvalue reference to a deduced type, but because C++ uses the syntax for a deduced rvalue parameter type for universal references (that also accepts an lvalue reference), it's not straight forward to do so.

`RequireRvalue` is an `std::enable_if`-style template that can be used to require that a parameter that would otherwise be a universal reference is a non-const rvalue reference or a value (which behaves the same).

`RequireRvalue` is an API for advanced use of the rs library. It is mostly useful when creating custom templated operators.

In rs, `RequireRvalue` is used in the built-in implementations of the Subscriber concept, for the `OnNext` method. [`OnNext`'s parameter must be an rvalue reference](specification.md#2-subscriber-code), and by always enforcing that in `OnNext` certain type errors are caught earlier than if `RequireRvalue` would not have been used.

**Example usage:**

```cpp
template <typename T, class = RequireRvalue<T>>
void UseRvalue(T &&t) {
  // ...
}
```

```cpp
UseRvalue(std::string("hello"));  // Valid

std::string a("a");
UseRvalue(std::move(a));  // Valid

std::string b("b");
UseRvalue(b);  // NOT valid
```


## `Scan(Accumulator, Mapper)`

**Defined in:** [`rs/scan.h`](../include/rs/scan.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `a, (a, b -> a) -> (Publisher[b] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#scan), [ReactiveX](http://reactivex.io/documentation/operators/scan.html)

**Description:** `Scan` is like [`Map(Mapper)`](#mapmapper), but the mapper function is given the previously emitted value as well. In order to have something when processing the first value, `Scan` takes the first "previously emitted value" as a parameter.

**Example usage:**

```cpp
// An operator that takes a Publisher of ints and returns a Publisher of ints
// where each output value is the sum of all previously seen values.
auto RunningSum() {
  return Scan(0, [](int accum, int val) { return accum + val; });
}
```

```cpp
// sums is a stream that emits 3, 5, 6  (it is 3, then 3+2, then 3+2+1)
auto sums = Pipe(
    Just(3, 2, 1),
    RunningSum());
```

**See also:** [`Map(Mapper)`](#mapmapper)


## `Skip(size_t)`

**Defined in:** [`rs/skip.h`](../include/rs/skip.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `size_t -> (Publisher[a] -> Publisher[a])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#skip), [ReactiveX](http://reactivex.io/documentation/operators/skip.html)

**Description:** Takes a Publisher and returns one that behaves like it, except that the first `n` elements are dropped. If the stream ends before that many elements are emitted, the stream finishes without emitting any element (it is not an error).

**Example usage:**

```cpp
// two_three_four is a stream that emits 2, 3, 4
auto two_three_four = Pipe(
    Just(0, 1, 2, 3, 4),
    Skip(2));
```

```cpp
// empty is a stream of ints that emits no values
auto empty = Pipe(
    Just(0, 1, 2),
    Skip(10));
```

**See also:** [`Filter(Predicate)`](#filterpredicate), [`SkipWhile(Predicate)`](#skipwhilepredicate), [`Take(Count)`](#takecount), [`TakeWhile(Predicate)`](#takewhilepredicate)


## `SkipWhile(Predicate)`

**Defined in:** [`rs/skip_while.h`](../include/rs/skip_while.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[a])`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/skipwhile.html)

**Description:** Takes a stream of values and returns a stream that has the same values in it except for the values that come before the first value for which the the predicate returns true; they are dropped.

**Example usage:**

```cpp
// first_positive_and_onward is a stream that emits 1, 0, -2, 1
auto first_positive_and_onward = Pipe(
    Just(-3, -2, -5, 1, 0, -2, 1),
    SkipWhile([](int x) { return x > 0; }));
```

**See also:** [`Filter(Predicate)`](#filterpredicate), [`Skip(size_t)`](#skipsize_t), [`Take(Count)`](#takecount), [`TakeWhile(Predicate)`](#takewhilepredicate)


## `Some(Predicate)`

**Defined in:** [`rs/some.h`](../include/rs/some.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[bool])`

**External documentation:** [RxMarbles](http://rxmarbles.com/#some)

**Description:** Operator that returns a stream that emits exactly one value: `true` if any of the input elements match the given predicate, `false` otherwise. As soon as an element is encountered that matches the predicate, the boolean is emitted and the input stream is cancelled.

**Example usage:**

```cpp
auto input = Just(1, 2, 5, 1, -3, 1);
auto has_negative = Pipe(
    input,
    Some([](int value) { return value < 0; }));
```

**See also:** [`All(Predicate)`](#allpredicate)


## `Splat(Functor)`

**Defined in:** [`rs/splat.h`](../include/rs/splat.h)

**Kind:** [Core Library API](#kind_core_library_api)

**[Type](#types):** `(a, b, ... -> c) -> (std::tuple<a, b, ...>&& -> c)`

**Description:** `Splat` is a helper function that can make it easier to access the individual elements of a tuple or a pair. What it does is similar to `std::tie`, but it is meant to be used in a different context. In cases where you would write:

```cpp
[](std::tuple<int, std::string> t) {
  auto &num = std::get<0>(t);
  auto &str = std::get<1>(t);
  ...
}
```

You could use `Splat` and instead write:

```cpp
Splat([](int num, std::string str) {
  ...
})
```

`Splat` works with all types for which `std::tuple_size` and `std::get` are defined: In addition to tuples it also works with `pair<>`s and `array<>`s.

**Example usage:**

`Splat` is particularly useful when dealing with streams that have tuples, for example because of [`Zip(Publisher...)`](#zippublisher). Instead of writing

```cpp
Pipe(
    Zip(Just(1, 2), Just("a", "b"))
    Map([](std::tuple<int, std::string> &&tuple) {
      return std::get<1>(tuple) + " " + std::to_string(std::get<0<>(tuple)));
    }));
```

which does not read particularly nicely, `Splat` allows you to write

```cpp
Pipe(
    Zip(Just(1, 2), Just("a", "b"))
    Map(Splat([](int num, std::string str) {
      return str + " " + std::to_string(num);
    })));
```

**See also:** [`Zip(Publisher...)`](#zippublisher)


## `Start(CreateValue...)`

**Defined in:** [`rs/start.h`](../include/rs/start.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `(void -> a), (void -> b), ... -> Publisher[a, b, ...]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/start.html)

**Description:** Constructs a Publisher that emits one value for each of the provided functors. The provided functors are invoked and their return values are emitted to the output stream.

`Start` is particularly useful if the emitted values are not copyable, for example if they are `std::unique_ptr`s. If they are copyable, consider using [`Just(Value...)`](#justvalue) instead; it has a simpler interface.

**Example usage:**

```cpp
auto one = Start([] { return 1; });

// It's possible to create a stream with more than one value
auto one_and_two = Start([] { return 1; }, [] { return 2; });
// It's possible to create a stream with more zero values; this is like Empty
auto empty = Start();

// The types of the values to emit may be different
auto different_types = one_hi = Start(
    [] { return 1; }, [] { return "hi"; });
```

**See also:** [`Empty()`](#empty), [`From(Container)`](#fromcontainer), [`Just(Value...)`](#justvalue)


## `StartWith(Value...)`

**Defined in:** [`rs/start_with.h`](../include/rs/start_with.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `a, b, ... -> (Publisher[c] -> Publisher[a, b, ..., c]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#startWith), [ReactiveX](http://reactivex.io/documentation/operators/startwith.html)

**Description:** Prepends the provided values to a stream.

The parameters of `StartWith` must be copyable. If they are not, consider using [`StartWithGet(MakeValue...)`](#startwithgetmakevalue).

**Example usage:**

```cpp
// stream is a stream that emits "hello", "world", 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    StartWith("hello", "world"));
```

```cpp
// It is possible to call StartWith with no parameters. Then it is a no-op
// operator.
//
// stream is a stream that emits 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    StartWith());
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Concat(Publisher...)`](#concatpublisher), [`EndWith(Value...)`](#endwithvalue), [`EndWithGet(MakeValue...)`](#endwithgetmakevalue), [`Prepend(Publisher...)`](#prependpublisher), [`StartWithGet(MakeValue...)`](#startwithgetmakevalue)


## `StartWithGet(MakeValue...)`

**Defined in:** [`rs/start_with.h`](../include/rs/start_with.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(void -> a), (void -> b), ... -> (Publisher[c] -> Publisher[a, b, ..., c]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#startWith), [ReactiveX](http://reactivex.io/documentation/operators/startwith.html)

**Description:** `StartWithGet` is like [`StartWith(Value...)`](#startwithvalue), but it takes functions that create the values to be emitted rather than the values directly. This can be used when the values are expensive to craete or when they are noncopyable.

**Example usage:***

```cpp
// stream is a stream that emits "hello", "world", 1, 2, 3
auto stream = Pipe(
    Just(1, 2, 3),
    StartWithGet([] { return "hello"; }, [] { return "world"; }));
```

**See also:** [`Append(Publisher...)`](#appendpublisher), [`Concat(Publisher...)`](#concatpublisher), [`EndWith(Value...)`](#endwithvalue), [`EndWithGet(MakeValue...)`](#endwithgetmakevalue), [`Prepend(Publisher...)`](#prependpublisher), [`StartWith(Value...)`](#startwithvalue)


## `Subscriber`

**Defined in:** [`rs/subscriber.h`](../include/rs/subscriber.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** One of the [requirements for Subscriber types](specification.md#2-subscriber-code) is that they must inherit `Subscriber` to signify the intent of being a Subscriber.

`Subscriber` itself is a class that does not do anything at all; its sole purpose is to make it so that Subscriber types have to declare that they want to be Subscribers.

`Subscriber` has a `protected` constructor. This makes it impossible to construct `Subscriber` objects directly; only subclasses of it can be instantiated.

**See also:** [`Publisher`](#publisher), [`Subscription`](#subscription)


## `Subscription`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:** One of the [requirements for Subscription types](specification.md#3-subscription-code) is that they must inherit `Subscription` to signify the intent of being a Subscription.

`Subscription` itself is a class that does not do anything at all; its sole purpose is to make it so that Subscription types have to declare that they want to be Subscriptions.

`Subscription` has a `protected` constructor. This makes it impossible to construct `Subscription` objects directly; only subclasses of it can be instantiated.

**See also:** [`Publisher`](#publisher), [`Subscriber`](#subscriber)


## `Sum()`

**Defined in:** [`rs/sum.h`](../include/rs/sum.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `void -> (Publisher[a] -> Publisher[SumType])`, where `SumType` is `int` by default and configurable by a template parameter to `Sum()`.

**External documentation:** [RxMarbles](http://rxmarbles.com/#sum), [ReactiveX](http://reactivex.io/documentation/operators/sum.html)

**Description:** Takes a stream and returns a stream that emits exactly one value: The sum of all values in the stream. `operator+` is used for adding values. By default the type of the emitted value is `int` but that can be customized.

**Example usage:**

```cpp
// sum is a Publisher that emits the sum of all numbers between 1 and 100
auto sum = Pipe(
    Range(1, 100),
    Sum());
```

```cpp
// It's possible to specify the type of the output value
auto sum = Pipe(
    Range(1, 100),
    Sum<double>());
```

**See also:** [`Reduce(Accumulator, Reducer)`](#reduceaccumulator-reducer)


## `Take(Count)`

**Defined in:** [`rs/take.h`](../include/rs/take.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `CountType -> (Publisher[a] -> Publisher[a])`, where `CountType` is a numeric type, for example `int`, `size_t` or `ElementCount`.

**External documentation:** [RxMarbles](http://rxmarbles.com/#take), [ReactiveX](http://reactivex.io/documentation/operators/take.html)

**Description:** Takes up to the given number of elements from the input stream. If the input stream completes earlier than that, the output stream also completes. When the target number of elements have been emitted, the input stream is cancelled.

**Example usage:**

```cpp
// sum is a Publisher that emits the sum of all numbers between 1 and 10
auto sum = Pipe(
    Range(1, 100),
    Take(10),
    Sum());
```

```cpp
// sum is a Publisher that emits 1
auto sum = Pipe(
    Just(1),
    Take(10),  // It is not an error if the input stream has too few elements.
    Sum());
```

**See also:** [`TakeWhile(Predicate)`](#takewhilepredicate)


## `TakeWhile(Predicate)`

**Defined in:** [`rs/take_while.h`](../include/rs/take_while.h)

**Kind:** [Operator Builder](#kind_operator_builder)

**[Type](#types):** `(a -> bool) -> (Publisher[a] -> Publisher[a])`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/takewhile.html)

**Description:** Takes elements from the input stream until the predicate returns `false` for an element: Then the output strem completes and the input stream is cancelled. If the input stream completes before the predicate returns `false`, the output stream also completes successfully.

**Example usage:**

```cpp
// positive is a Publisher that emits 3, 1, 5
auto positive = Pipe(
    Just(3, 1, 5, -1, 2, 4, 5),
    TakeWhile([](int x) { return x > 0; }));
```

```cpp
// sum is a Publisher that emits 3, 1, 5
auto sum = Pipe(
    Just(3, 1, 5),
    // It is not an error if the input stream completes before the predicate
    // returns false.
    TakeWhile([](int x) { return x > 0; }));
    Sum());
```

**See also:** [`Take(Count)`](#takecount)


## `Throw(Exception)`

**Defined in:** [`rs/throw.h`](../include/rs/throw.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `ExceptionType -> Publisher[]`, where `ExceptionType` is anything that can be thrown as an exception in C++ except an `std::exception_ptr`.

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/empty-never-throw.html)

**Description:** Creates a Publisher that immediately fails with the given error. This variant of `Throw` wraps the given error by calling `std::make_exception_ptr` on its parameter.

**Example usage:**

```cpp
auto fail = Throw(std::runtime_error("fail"));
```

```cpp
// An implementation of the First operator
auto First() {
  // Use IfEmpty to ensure that there is at least one value.
  return BuildPipe(
      Take(1),
      IfEmpty(Throw(std::out_of_range("fail"))));
}
```

**See also:** [`Empty()`](#empty), [`Just(Value...)`](#justvalue), [`Never()`](#never), [`Throw(const std::exception_ptr &)`](#throwconst-stdexception_ptr-)


## `Throw(const std::exception_ptr &)`

**Defined in:** [`rs/throw.h`](../include/rs/throw.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `std::exception_ptr -> Publisher[]`

**External documentation:** [ReactiveX](http://reactivex.io/documentation/operators/empty-never-throw.html)

**Description:** Creates a Publisher that immediately fails with the given error.

**Example usage:**

```cpp
auto fail = Throw(std::make_exception_ptr(std::runtime_error("fail")));
```

The documentation for [`Throw(Exception)`](#throwexception) has more examples.

**See also:** [`Empty()`](#empty), [`Just(Value...)`](#justvalue), [`Never()`](#never), [`Throw(Exception)`](#throwexception)


## `VirtualSubscription`

**Defined in:** [`rs/subscription.h`](../include/rs/subscription.h)

**Kind:** [Core Library API](#kind_core_library_api)

**Description:**

`VirtualSubscription` is a helper class that wraps any Subscription object without changing its behavior. What it adds is that it implements the [`PureVirtualSubscription`](#purevirtualsubscription) interface, which is useful for example when implementing [`AnySubscription`](#anysubscription) but it can also be useful to operator implementations: It is possible to have a `PureVirtualSubscription`](#purevirtualsubscription) pointer or reference to it in situations where the full type cannot be used due to circular references. See for example [`Map`](#map) and [`Filter`](#filter).

**See also:** [`AnySubscription`](#anysubscription), [`Subscription`](#subscription), [`VirtualSubscription`](#virtualsubscription)


## `Zip(Publisher...)`

**Defined in:** [`rs/zip.h`](../include/rs/zip.h)

**Kind:** [Operator](#kind_operator)

**[Type](#types):** `Publisher[a], Publisher[b], ... -> Publisher[std::tuple<a, b, ...>]`

**External documentation:** [RxMarbles](http://rxmarbles.com/#zip), [ReactiveX](http://reactivex.io/documentation/operators/zip.html)

**Description:** Takes a number of input Publishers and returns a Publisher that emits tuples of values from all the input streams. For example, zipping `[1, 2, 3]` with `[a, b, c]` emits `[(1, a), (2, b), (3, c)]`.

If the input streams emit different numbers of elements, the resulting stream emits as many values as the smallest input stream. The other values are dropped.

In code that performs server/client style RPC/REST communication, `Zip` is very handy when making two or more requests in parallel and then continue when all of the requests have finished.

Many functional programming languages and libraries has a version of this function that operates on lists that is also called `zip`, for example [Haskell](http://hackage.haskell.org/package/base-4.9.1.0/docs/Prelude.html#v:zip).

**Example usage:**

```cpp
// zipped is a Publisher that emits "a 1", "b 2"
auto zipped = Pipe(
    Zip(Just(1, 2), Just("a", "b"))
    Map([](std::tuple<int, std::string> &&tuple) {
      return std::get<1>(tuple) + " " + std::to_string(std::get<0<>(tuple)));
    }));
```

The example above can be written in a nicer way using [`Splat(Functor)`](#splatfunctor):

```cpp
// zipped is a Publisher that emits "a 1", "b 2"
auto zipped = Pipe(
    Zip(Just(1, 2), Just("a", "b"))
    Map(Splat([](int num, std::string str) {
      return str + " " + std::to_string(num);
    })));
```

A more real world-like use case:

```cpp
std::string RenderShoppingCart(
    const UserInfo &user_info,
    const std::vector<ShoppingCartItem> &cart_items) {
  return "...";
}

Publisher<std::string> RequestAndRenderShoppingCart(
    const Backend &backend,
    const std::string &user_id) {
  Publisher<UserInfo> user_info_request = backend.GetUserInfo(user_id);
  Publisher<std::vector<ShoppingCartItem>> cart_items_request =
      backend.GetShoppingCartItems(user_id);

  return Publisher<std::string>(Pipe(
      Zip(user_info_request, cart_items_request),
      Map(Splat(&RenderShoppingCart)));
}
```

**See also:** [`Merge(Publisher...)`](#mergepublisher), [`Splat(Functor)`](#splatfunctor)


## Legend

### Kinds

* <em><a name="kind_operator">Operator</a></em>: Function that returns a Publisher.
* <em><a name="kind_operator_builder">Operator Builder</a></em>: Function that returns an Operator.
* <em><a name="kind_operator_builder_builder">Operator Builder Builder</a></em>: Function that returns an Operator Builder.
* <em><a name="kind_core_library_api">Core Library API</a></em>: A function or type that is part of the core of rs, not an operator.


### Types

Each operator described here has a section that describes its type. Because C++ does not have a syntax for concepts, these descrpitions have a made-up syntax:

* `Publisher[x]` is a C++ type that fulfills the requirements of the Publisher concept that publishes elements of type `x`.
* `Publisher[x, y]` is a Publisher that can emit elements of type `x` and `y`.
* `Subscriber[x]` is a C++ type that fulfills the requirements of the Subscriber concept that publishes elements of type `x`.
* `Subscription[]` is a C++ type that fulfills the requirements of the Subscription concept.
* `x -> y` is a function that takes `x` as a parameter and returns `y`.
* `x, y -> z` is a function with two parameters of type `x` and `y` and returns `z`.
* `x? -> y` is a function that takes `x` as an optional parameter (it's also allowed to pass no parameters to it) and returns `y`.
* `bool`, `size_t` and `std::exception_ptr&&` refer to the C++ types with the same name.
* `void` is the C++ `void` type. It is also used here to denote functions that take no parameters: for example, `void -> bool` a function that takes no parameters and returns a `bool`.
* Letters such as `a`, `b` and `c` can represent any type. If a letter occurs more than once in a type declaration, all occurences refer to the same type.
