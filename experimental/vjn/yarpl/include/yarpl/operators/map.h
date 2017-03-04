#pragma once

#include <yarpl/flowable.h>
#include <reactivestreams/subscriber.h>

namespace yarpl { namespace operators {

using ::reactivestreams::Subscriber;

template<typename T, typename Transform>
class Mapper : public Flowable<T>, public ::reactivestreams::Subscriber<T> {
  using Flowable = ::yarpl::Flowable<T>;
  using Subscriber = ::reactivestreams::Subscriber<T>;
  using Subscription = ::reactivestreams::Subscription<T>;
public:
  Mapper(Transform&& transform, std::shared_ptr<Flowable> upstream)
      : forwarder_(std::unique_ptr<Forwarder>(
          new Forwarder(std::forward<Transform>(transform)))),
        upstream_(upstream) {}

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    // TODO(vjn): check that subscriber_.get() == nullptr?
    forwarder_->set_subscriber(std::move(subscriber));
    upstream_->subscribe(std::move(forwarder_));
  }

  virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
      Scheduler& scheduler) override {
  }
private:
  class Forwarder : public Subscriber {
  public:
    Forwarder(Transform&& transform)
        : transform_(std::forward<Transform>(transform)) {}

    void set_subscriber(std::unique_ptr<Subscriber> subscriber) {
      subscriber_ = std::move(subscriber);
    }

    virtual void on_next(const T& value) override {
      subscriber_->on_next(transform_(value));
    }

    virtual void on_error(const std::exception& error) override {
      subscriber_->on_error(error);
    }

    virtual void on_complete() override {
      subscriber_->on_complete();
    }

    virtual void on_subscribe(Subscription* subscription) override {
      subscriber_->on_subscribe(subscription);
    }

  private:
    Transform transform_;
    std::unique_ptr<Subscriber> subscriber_;
  };

  std::unique_ptr<Forwarder> forwarder_;
  const std::shared_ptr<Flowable> upstream_;
};

}}  // yarpl::operators
