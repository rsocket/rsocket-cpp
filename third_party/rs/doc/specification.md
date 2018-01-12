# `rs` Reactive Streams specification

This document is a version of the [Reactive Streams specification for the JVM](https://github.com/reactive-streams/reactive-streams-jvm) that is adapted for C++.


## Goals, Design and Scope

Please refer to [the Reactive Streams for JVM's documentation](https://github.com/reactive-streams/reactive-streams-jvm#goals-design-and-scope).


## API Components

The API consists of the following components:

1. Publisher
2. Subscriber
3. Subscription

In the rs library, these are not concrete types; rather, they are concepts: Any type can be a Publisher, a Subscriber or a Subscription as long as they fulfill all the requirements of that concept.

There are type predicates for each concept:

* `IsPublisher<T>` is a `constexpr` expression that evaluates to `true` if `T` claims to be a Publisher, otherwise `false`.
* `IsSubscriber<T>` is a `constexpr` expression that evaluates to `true` if `T` claims to be a Subscriber, otherwise `false`.
* `IsSubscription<T>` is a `constexpr` expression that evaluates to `true` if `T` claims to be a Subscription, otherwise `false`.

It is sometimes useful to have a concrete type that can hold an object that is a Publisher, a Subscriber or a Subscription. Since these concepts don't have a single concrete type this is not possible to do directly. To make that possible, there are type erasure wrappers for each concept:

`Publisher<T> erased_publisher(publisher)` defines a `Publisher<T>` object that is itself a Publisher from `publisher`, regardless of which concrete type `publisher` has. This can be useful to be able to return a Publisher object from a C++ implementation file without having to expose which type of Publisher it is in the header file.

Similarly, there are `Subscriber<T>` and `Subscription` type erasure wrappers for Subscriber and Subscription.

The type erasure wrappers use `virtual` method calls, so using them incurs some overhead.


## Specification for the Publisher, Subscriber and Subscription concepts


### Glossary

The specifications below use binding words in capital letters from https://www.ietf.org/rfc/rfc2119.txt

| Term                      | Definition                                                                                             |
| ------------------------- | ------------------------------------------------------------------------------------------------------ |
| <a name="term_signal">Signal</a> | As a noun: one of the `OnNext`, `OnComplete`, `OnError`, `Request(n)`, `Cancel` or Subscription destructor (which implies `Cancel`) methods. As a verb: calling/invoking a signal. |
| <a name="term_demand">Demand</a> | As a noun, the aggregated number of elements requested by a Subscriber which is yet to be delivered (fulfilled) by the Publisher. As a verb, the act of `Request`-ing more elements. |
| <a name="term_return_normally">Return normally</a> | Only ever returns a value of the declared type to the caller. The only legal way to signal failure to a `Subscriber` is via the `OnError` method.|
| <a name="term_responsivity">Responsivity</a> | Readiness/ability to respond. In this document used to indicate that the different components should not impair each others ability to respond. |
| <a name="term_non-obstructing">Non-obstructing</a> | Quality describing a method which is as quick to execute as possible—on the calling thread. This means, for example, avoids heavy computations and other things that would stall the caller´s thread of execution. |
| <a name="term_terminal_state">Terminal state</a> | For a Publisher: When `OnComplete` or `OnError` has been signalled. For a Subscriber: When an `OnComplete` or `OnError` has been received. |
| <a name="term_nop">NOP</a> | Execution that has no detectable effect to the calling thread, and can as such safely be called any number of times. |


### 1. Publisher ([Code](../include/rs/publisher.h))

A *Publisher* is a provider of a potentially unbounded number of sequenced elements, publishing them according to the demand received from its Subscriber(s). Having a Publisher does not in itself mean that any data is being streamed; many Publishers wait with doing anything until the `Subscribe` method is invoked.

A Publisher MUST publicly inherit the `Publisher` class and MUST offer a `Subscribe` method for subscribing to the stream that takes an rvalue reference to a Subscriber and returns a Subscription. Because there is no concrete base type for all Subscribers, this method must be a template.

That `Subscribe` takes an rvalue reference to a Subscription implies that the Publisher takes over ownership of the Subscriber when it is subscribed to.

In response to a call to `Subscribe` the possible invocation sequences for methods on the `Subscriber` are given by the following protocol:

```
OnNext* (OnError | OnComplete)? ~Subscriber
```

This means that the subscriber will receive a possibly unbounded number of `OnNext` signals (as requested with the `Request` method) followed by an `OnError` signal if there is a failure, or an `OnComplete` signal when no more elements are available—all as long as the `Subscription` is not cancelled. Finally, the Subscriber is destroyed.

| ID                        | Rule                                                                                                   |
| ------------------------- | ------------------------------------------------------------------------------------------------------ |
| <a name="1.1">1</a>       | The total number of `OnNext`´s signalled by a `Publisher` to a `Subscriber` MUST be less than or equal to the total number of elements requested by that `Subscriber`´s `Subscription` at all times. |
| [:bulb:](#1.1 "1.1 explained") | *The intent of this rule is to make it clear that Publishers cannot signal more elements than Subscribers have requested. There’s an implicit, but important, consequence to this rule: Since demand can only be fulfilled after it has been received, there’s a happens-before relationship between requesting elements and receiving elements.* |
| <a name="1.2">2</a>       | A `Publisher` MAY signal fewer `OnNext` than requested and terminate the `Subscription` by calling `OnComplete` or `OnError`. |
| [:bulb:](#1.2 "1.2 explained") | *The intent of this rule is to make it clear that a Publisher cannot guarantee that it will be able to produce the number of elements requested; it simply might not be able to produce them all; it may be in a failed state; it may be empty or otherwise already completed.* |
| <a name="1.3">3</a>       | `OnNext`, `OnError` and `OnComplete` signaled to a `Subscriber` MUST be signaled in a `thread-safe` manner—and if performed by multiple threads—use [external synchronization](#term_ext_sync). |
| [:bulb:](#1.3 "1.3 explained") | *The intent of this rule is to make it clear that [external synchronization](#term_ext_sync) must be employed if the Publisher intends to send signals from multiple/different threads.* |
| <a name="1.4">4</a>       | If a `Publisher` fails it MUST signal an `OnError`. |
| [:bulb:](#1.4 "1.4 explained") | *The intent of this rule is to make it clear that a Publisher is responsible for notifying its Subscribers if it detects that it cannot proceed—Subscribers must be given a chance to clean up resources or otherwise deal with the Publisher´s failures.* |
| <a name="1.5">5</a>       | If a `Publisher` terminates successfully (finite stream) it MUST signal an `OnComplete`. |
| [:bulb:](#1.5 "1.5 explained") | *The intent of this rule is to make it clear that a Publisher is responsible for notifying its Subscribers that it has reached a [terminal state](#term_terminal-state)—Subscribers can then act on this information; clean up resources, etc.* |
| <a name="1.6">6</a>       | If a `Publisher` signals either `OnError` or `OnComplete` on a `Subscriber`, that `Subscriber`’s `Subscription` MUST be considered cancelled. |
| [:bulb:](#1.6 "1.6 explained") | *The intent of this rule is to make sure that a Subscription is treated the same no matter if it was cancelled, the Publisher signalled OnError or OnComplete.* |
| <a name="1.7">7</a>       | Once a [terminal state](#term_terminal-state) has been signaled (`OnError`, `OnComplete`) it is REQUIRED that no further signals occur. |
| [:bulb:](#1.7 "1.7 explained") | *The intent of this rule is to make sure that OnError and OnComplete are the final states of an interaction between a Publisher and Subscriber pair.* |
| <a name="1.8">8</a>       | If a `Subscription` is cancelled its `Subscriber` MUST eventually stop being signaled. |
| [:bulb:](#1.8 "1.8 explained") | *The intent of this rule is to make sure that Publishers respect a Subscriber’s request to cancel a Subscription. The reason for *eventually* is because signals can have propagation delay due to being asynchronous.* |
| <a name="1.9">9</a>       | `Publisher.Subscribe` MUST [return normally](#term_return_normally). The only legal way to signal failure (or reject the `Subscriber`) is by calling `OnError`. |
| [:bulb:](#1.9 "1.9 explained") | *The intent of this rule is to make sure that subscribing to a Publisher never throws. |
| <a name="1.10">10</a>     | A `Publisher` MAY support multiple `Subscriber`s and decides whether each `Subscription` is unicast or multicast. |
| [:bulb:](#1.10 "1.10 explained") | *The intent of this rule is to give Publisher implementations the flexibility to decide how many, if any, Subscribers they will support, and how elements are going to be distributed.* |
| <a name="1.11">11</a>     | A `Publisher` MUST make sure that the subscriptions keep functioning after the Publisher has been destroyed. |
| [:bulb:](#1.11 "1.11 explained") | *A Publisher object's lifetime does not necessesarily extend beyond the subscriptions that it has created. Users of a Publisher MAY assume that creating a short-lived Publisher object does not cause subscriptions to be prematurely cancelled.* |



### 2. Subscriber ([Code](../include/rs/subscriber.h))

A *Subscriber* is a receiver of a potentially unbounded number of sequenced elements. In order to get to the elements of a Publisher, it needs to be given a Subscriber, which it will then notify of its elements.

A Subscriber MUST publicly inherit the `Subscriber` class, MUST be movable and MUST have the following methods:

* `void OnNext(T &&t);`: One such method for each type that the subscriber accepts. The `t` parameter is an rvalue reference; it is not legal to pass lvalue references to `OnNext`. There could potentially be zero `OnNext` methods.
* `void OnError(std::exception_ptr &&error);`
* `void OnComplete();`

| ID                        | Rule                                                                                                   |
| ------------------------- | ------------------------------------------------------------------------------------------------------ |
| <a name="2.1">1</a>       | A `Subscriber` MUST indicate demand via `Subscription.Request(ElementCount n)` to receive `OnNext` signals. |
| [:bulb:](#2.1 "2.1 explained") | *The intent of this rule is to establish that it is the responsibility of the Subscriber to indicate when, and how many, elements it is able and willing to receive.* |
| <a name="2.2">2</a>       | If a `Subscriber` suspects that its processing of signals will negatively impact its `Publisher`’s responsivity, it is RECOMMENDED that it asynchronously dispatches its signals. |
| [:bulb:](#2.2 "2.2 explained") | *The intent of this rule is that a Subscriber should [not obstruct](#term_non-obstructing) the progress of the Publisher from an execution point-of-view. In other words, the Subscriber should not starve the Publisher from CPU cycles.* |
| <a name="2.3">3</a>       | `Subscriber.OnComplete()` and `Subscriber.OnError(std::exception_ptr &&error)` MUST NOT call any methods on the `Subscription` or the `Publisher`. |
| [:bulb:](#2.3 "2.3 explained") | *The intent of this rule is to prevent cycles and race-conditions—between Publisher, Subscription and Subscriber—during the processing of completion signals.* |
| <a name="2.4">4</a>       | `Subscriber.OnComplete()` and `Subscriber.OnError(std::exception_ptr &&error)` MUST consider the Subscription cancelled after having received the signal. |
| [:bulb:](#2.4 "2.4 explained") | *The intent of this rule is to make sure that Subscribers respect a Publisher’s [terminal state](#term_terminal-state) signals. A Subscription is simply not valid anymore after an OnComplete or OnError signal has been received.* |
| <a name="2.5">5</a>       | A `Subscriber` MUST call `Subscription.Cancel()` (or destroy its `Subscription`, which implies cancellation) if the `Subscription` is no longer needed. |
| [:bulb:](#2.5 "2.5 explained") | *The intent of this rule is to establish that Subscribers cannot just leave Subscriptions when they are no longer needed, they have to call `Cancel` so that resources held by that Subscription can be reclaimed in a timely manner. An example of this would be a Subscriber which is only interested in a specific element, which would then cancel its Subscription to signal its completion to the Publisher.* |
| <a name="2.6">6</a>       | A `Subscriber` MUST ensure that all calls on its `Subscription` take place from the same thread or provide for respective [external synchronization](#term_ext_sync). |
| [:bulb:](#2.6 "2.6 explained") | *The intent of this rule is to establish that [external synchronization](#term_ext_sync) must be added if a Subscriber will be using a Subscription concurrently by two or more threads.* |
| <a name="2.7">7</a>       | A `Subscriber` MUST be prepared to receive one or more `OnNext` signals after having cancelled its `Subscription` if there are still requested elements pending [see [3.11](#3.11)]. Destroying a `Subscription` does not guarantee to perform the underlying cleaning operations immediately. |
| [:bulb:](#2.7 "2.7 explained") | *The intent of this rule is to highlight that there may be a delay between cancelling a `Subscription` and the Publisher seeing that.* |
| <a name="2.8">8</a>       | A `Subscriber` MUST be prepared to receive an `OnComplete` signal with or without a preceding `Subscription.Request(ElementCount n)` call. |
| [:bulb:](#2.8 "2.8 explained") | *The intent of this rule is to establish that completion is unrelated to the demand flow—this allows for streams which complete early, and obviates the need to *poll* for completion.* |
| <a name="2.9">9</a>     | A `Subscriber` MUST be prepared to receive an `OnError` signal with or without a preceding `Subscription.Request(ElementCount n)` call. |
| [:bulb:](#2.9 "2.9 explained") | *The intent of this rule is to establish that Publisher failures may be completely unrelated to signalled demand. This means that Subscribers do not need to poll to find out if the Publisher will not be able to fulfill its requests.* |
| <a name="2.10">10</a>     | A `Subscriber` MUST make sure that all calls on its [signal](#term_signal) methods happen-before the processing of the respective signals. I.e. the Subscriber must take care of properly publishing the signal to its processing logic. |
| [:bulb:](#2.10 "2.10 explained") | *The intent of this rule is to establish that it is the responsibility of the Subscriber implementation to make sure that asynchronous processing of its signals are thread safe. See [JMM definition of Happens-Before in section 17.4.5](https://docs.oracle.com/javase/specs/jls/se8/html/jls-17.html#jls-17.4.5).* |
| <a name="2.11">11</a>     | Calling `OnSubscribe`, `OnNext`, `OnError` or `OnComplete` MUST [return normally](#term_return_normally). The only legal way for a `Subscriber` to signal failure is by cancelling its `Subscription`. In the case that this rule is violated, any associated `Subscription` to the `Subscriber` MUST be considered as cancelled, and the caller MUST raise this error condition in a fashion that is adequate for the runtime environment. |
| [:bulb:](#2.11 "2.11 explained") | *The intent of this rule is to establish the semantics for the methods of Subscriber and what the Publisher is allowed to do in which case this rule is violated. «Raise this error condition in a fashion that is adequate for the runtime environment» could mean logging the error—or otherwise make someone or something aware of the situation—as the error cannot be signalled to the faulty Subscriber.* |


### 3. Subscription ([Code](../include/rs/subscription.h))

A *Subscription* is a handle that is provided by the Publisher when a subscription is made against it. Calling `Request` signals to the publisher that the Subscriber is ready to receive data. Calling `Cancel` or destroying the Subscription object implies that the Subscription is cancelled and the Publisher will eventually stop emitting elements to the Subscriber.

A Subscription:

* MUST publicly inherit the `Subscription` class,
* MUST be default constructible,
* MUST be move assignable and move constructible,
* MUST have a `void Request(ElementCount count);` method, and
* MUST have a `void Cancel();` method.

On default constructed Subscription objects, calls to `Request` and `Cancel` MUST be [NOPs](#term_nop).

| ID                        | Rule                                                                                                   |
| ------------------------- | ------------------------------------------------------------------------------------------------------ |
| <a name="3.1">1</a>       | `Subscription.Request`, `Subscription.Cancel` and a `Subscription`'s destructor MUST only be called inside of its `Subscriber` context. |
| [:bulb:](#3.1 "3.1 explained") | *The intent of this rule is to establish that a Subscription represents the unique relationship between a Subscriber and a Publisher [see [2.12](#2.12)]. The Subscriber is in control over when elements are requested and when more elements are no longer needed.* |
| <a name="3.2">2</a>       | The `Subscription` MUST allow the `Subscriber` to call `Subscription.Request` synchronously from within `OnNext`. |
| [:bulb:](#3.2 "3.2 explained") | *The intent of this rule is to make it clear that implementations of `Request` must be reentrant, to avoid stack overflows in the case of mutual recursion between `Request` and `OnNext` (and eventually `OnComplete` / `OnError`). This implies that Publishers can be `synchronous`, i.e. signalling `OnNext`:s on the thread which calls `Request`.* |
| <a name="3.3">3</a>       | `Subscription.Request` MUST place an upper bound on possible synchronous recursion between `Publisher` and `Subscriber`. |
| [:bulb:](#3.3 "3.3 explained") | *The intent of this rule is to complement [see [3.2](#3.2)] by placing an upper limit on the mutual recursion between `Request` and `OnNext` (and eventually `OnComplete` / `OnError`). Implementations are RECOMMENDED to limit this mutual recursion to a depth of `1` (ONE)—for the sake of conserving stack space. An example for undesirable synchronous, open recursion would be Subscriber.OnNext -> Subscription.Request -> Subscriber.OnNext -> …, as it otherwise will result in blowing the calling Thread´s stack.* |
| <a name="3.4">4</a>       | `Subscription.Request` SHOULD respect the responsivity of its caller by returning in a timely manner. |
| [:bulb:](#3.4 "3.4 explained") | *The intent of this rule is to establish that `Request` is intended to be a [non-obstructing](#term_non-obstructing) method, and should be as quick to execute as possible on the calling thread, so avoid heavy computations and other things that would stall the caller´s thread of execution.* |
| <a name="3.5">5</a>       | `Subscription.Cancel` and `Subscription`'s destructor MUST respect the responsivity of its caller by returning in a timely manner. |
| [:bulb:](#3.5 "3.5 explained") | *The intent of this rule is to establish that the `Cancel` method and the destructor are intended to be [non-obstructing](#term_non-obstructing) methods, and should be as quick to execute as possible on the calling thread, so avoid heavy computations and other things that would stall the caller's thread of execution.* |
| <a name="3.6">6</a>       | After the `Subscription` is cancelled, additional `Subscription.Request(ElementCount n)` must be MUST be [NOPs](#term_nop). |
| [:bulb:](#3.6 "3.6 explained") | *The intent of this rule is to establish a causal relationship between cancellation of a subscription and the subsequent non-operation of requesting more elements.* |
| <a name="3.7">7</a>       | After the `Subscription` is cancelled, additional `Subscription.cancel()` MUST be [NOPs](#term_nop). |
| [:bulb:](#3.7 "3.7 explained") | *The intent of this rule is superseded by [3.5](#3.5).* |
| <a name="3.8">8</a>       | While the `Subscription` is not cancelled, `Subscription.Request(ElementCount n)` MUST register the given number of additional elements to be produced to the respective subscriber. |
| [:bulb:](#3.8 "3.8 explained") | *The intent of this rule is to make sure that `Request`-ing is an additive operation, as well as ensuring that a request for elements is delivered to the Publisher.* |
| <a name="3.9">9</a>     | While the `Subscription` is not cancelled, `Subscription.Request(ElementCount n)` MAY synchronously call `OnNext` on this (or other) subscriber(s). |
| [:bulb:](#3.9 "3.9 explained") | *The intent of this rule is to establish that it is allowed to create synchronous Publishers, i.e. Publishers who execute their logic on the calling thread.* |
| <a name="3.10">10</a>     | While the `Subscription` is not cancelled, `Subscription.Request(ElementCount n)` MAY synchronously call `OnComplete` or `OnError` on this (or other) subscriber(s). |
| [:bulb:](#3.10 "3.10 explained") | *The intent of this rule is to establish that it is allowed to create synchronous Publishers, i.e. Publishers who execute their logic on the calling thread.* |
| <a name="3.11">11</a>     | While the `Subscription` is not cancelled, `Subscription.Cancel` (and destroying the `Subscription`) MUST request the `Publisher` to eventually stop signaling its `Subscriber`. The operation is NOT REQUIRED to affect the `Subscription` immediately. |
| [:bulb:](#3.11 "3.11 explained") | *The intent of this rule is to establish that the desire to cancel a Subscription is eventually respected by the Publisher, acknowledging that it may take some time before the signal is received.* |
| <a name="3.12">12</a>     | While the `Subscription` is not cancelled, `Subscription.Cancel` (and destroying the `Subscription`) MUST request the `Publisher` to eventually drop any references to the corresponding subscriber. |
| [:bulb:](#3.12 "3.12 explained") | *The intent of this rule is to make sure that Subscribers can be properly destroyed after their subscription no longer being valid.* |
| <a name="3.13">13</a>     | While the `Subscription` is not cancelled, `Subscription.Cancel` (and destroying the `Subscription`) MAY cause the `Publisher`, if stateful, to transition into the `shut-down` state if no other `Subscription` exists at this point [see [1.8](#1.8)]. |
| [:bulb:](#3.13 "3.13 explained") | *The intent of this rule is to allow for Publishers to signal `OnComplete` or `OnError` for new Subscribers in response to a cancellation signal from an existing Subscriber.* |
| <a name="3.14">14</a>     | Calling `Subscription.Cancel` MUST [return normally](#term_return_normally). |
| [:bulb:](#3.14 "3.14 explained") | *The intent of this rule is to disallow implementations to throw exceptions in response to `Cancel` being called.* |
| <a name="3.15">15</a>     | Calling `Subscription.Request` MUST [return normally](#term_return_normally). |
| [:bulb:](#3.15 "3.15 explained") | *The intent of this rule is to disallow implementations to throw exceptions in response to `Request` being called.* |
| <a name="3.16">16</a>     | A `Subscription` MUST support an unbounded number of calls to `Request` and MUST support a demand up to 2^63-1 (`std::numeric_limits<long long>::max()`). A demand equal or greater than 2^63-1 (`std::numeric_limits<long long>::max()`) MAY be considered by the `Publisher` as “effectively unbounded”. |
| [:bulb:](#3.16 "3.16 explained") | *The intent of this rule is to establish that the Subscriber can request an unbounded number of elements, in any increment above 0, in any number of invocations of `Request`. As it is not feasibly reachable with current or foreseen hardware within a reasonable amount of time to fulfill a demand of 2^63-1, it is allowed for a Publisher to stop tracking demand beyond this point.* |


## Subscriber controlled queue bounds

One of the underlying design principles is that all buffer sizes are to be bounded and these bounds must be *known* and *controlled* by the subscribers (subscriber in this context refers to the entity that called the subscribe method on the Publisher, likely but not necessarily the same thing as the Subscriber object). These bounds are expressed in terms of *element count* (which in turn translates to the invocation count of onNext). Any implementation that aims to support infinite streams (especially high output rate streams) needs to enforce bounds all along the way to avoid out-of-memory errors and constrain resource usage in general.

Since back-pressure is mandatory the use of unbounded buffers can be avoided. In general, the only time when a queue might grow without bounds is when the publisher side maintains a higher rate than the subscriber for an extended period of time, but this scenario is handled by backpressure instead.

Queue bounds can be controlled by a subscriber indicating demand for the appropriate number of elements. At any point in time the subscriber knows:

* the total number of elements requested: `P`
* the number of elements that have been processed: `N`

Then the maximum number of elements that may arrive—until more demand is indicated to the Publisher—is `P - N`. In the case that the subscriber also knows the number of elements B in its input buffer then this bound can be refined to `P - B - N`.

These bounds must be respected by a publisher independent of whether the source it represents can be backpressured or not. In the case of sources whose production rate cannot be influenced—for example clock ticks or mouse movement—the publisher must choose to either buffer or drop elements to obey the imposed bounds.

Subscribers indicating a demand for one element after the reception of an element effectively implement a Stop-and-Wait protocol where the demand indication is equivalent to acknowledgement. By providing demand for multiple elements the cost of acknowledgement is amortized. It is worth noting that the subscriber is allowed to indicate demand at any point in time, allowing it to avoid unnecessary delays between the publisher and the subscriber (i.e. keeping its input buffer filled without having to wait for full round-trips).


## Acknowledgements

Much of this documentation is derived from the [documentation for Reactive streams for the JVM](https://github.com/reactive-streams/reactive-streams-jvm). Thanks!
