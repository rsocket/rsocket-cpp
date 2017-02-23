#include <iostream>
#include <string>

//#include <yarpl/Flowable.h>

// using namespace yarpl::flowable;

class FlowableExamples {
public:
  static void run();
};

/*********************** ASubscription **************/

class ASubscription {
public:
  virtual ~ASubscription(){};
  virtual void cancel() = 0;
  virtual void request(uint64_t n) = 0;
};

/*********************** ASubscriber ****************/

template <typename T> class ASubscriber {
public:
  virtual ~ASubscriber(){};
  virtual void onNext(const T &value) = 0;
  virtual void onError(const std::exception &e) = 0;
  virtual void onComplete() = 0;
  virtual void onSubscribe(std::unique_ptr<ASubscription>) = 0;
};

/*********************** AFlowable ******************/

template <typename T> class AFlowable {
  std::function<void(std::unique_ptr<ASubscriber<T>>)> onSubscribe;

public:
  ~AFlowable();
  AFlowable(std::function<void(std::unique_ptr<ASubscriber<T>>)> onSubscribe);
  static std::unique_ptr<AFlowable<T>>
  create(std::function<void(std::unique_ptr<ASubscriber<T>>)> onSubscribe);
  void subscribe(std::unique_ptr<ASubscriber<T>>);
};

template <typename T> AFlowable<T>::~AFlowable() {
  std::cout << "AFlowable DESTROYED" << std::endl;
}

template <typename T>
AFlowable<T>::AFlowable(std::function<void(std::unique_ptr<ASubscriber<T>>)> os)
    : onSubscribe(os) {
  std::cout << "AFlowable CREATED" << std::endl;
};

template <typename T>
std::unique_ptr<AFlowable<T>> AFlowable<T>::create(
    std::function<void(std::unique_ptr<ASubscriber<T>>)> onSubscribe) {
  return std::make_unique<AFlowable>(onSubscribe);
}

template <typename T>
void AFlowable<T>::subscribe(std::unique_ptr<ASubscriber<T>> o) {
  // when subscribed to, invoke the `onSubscribe` function
  onSubscribe(std::move(o));
}

/*********************** Experimenting **************/

void FlowableExamples::run() {
  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;

  class MySubscriber : public ASubscriber<int> {
    std::unique_ptr<ASubscription> theSubscription;

  public:
    MySubscriber() { std::cout << "MySubscriber CREATED" << std::endl; }
    ~MySubscriber() { std::cout << "MySubscriber DESTROYED" << std::endl; }
    void onNext(const int &value) {
      std::cout << "  onNext received " << value << std::endl;
    }
    void onError(const std::exception &e) {}
    void onComplete() {}
    void onSubscribe(std::unique_ptr<ASubscription> s) {
      theSubscription = std::move(s);
      theSubscription->request(10);
    }
  };

  class MySubscription : public ASubscription {
    std::weak_ptr<ASubscriber<int>> s;

  public:
    ~MySubscription() { std::cout << "MySubscription DESTROYED" << std::endl; }
    MySubscription(std::weak_ptr<ASubscriber<int>> _s) : s(std::move(_s)) {
      std::cout << "MySubscription CREATED" << std::endl;
    };
    void cancel() {}
    void request(uint64_t n) {
      auto sp = s.lock();
      if (sp) {
        // do stuff
        sp->onNext(1);
      }
    }
  };

  {
    std::cout << "--------- MySubscriber? " << std::endl;
    auto b = std::make_unique<MySubscriber>();
    b->onComplete();
  }
  std::cout << "--------- MySubscriber? " << std::endl;

  {
    AFlowable<int>::create([](std::unique_ptr<ASubscriber<int>> s) {
      std::cout << "AFlowable onSubscribe START" << std::endl;
      std::shared_ptr<ASubscriber<int>> sharedSubscriber = std::move(s);
      std::weak_ptr<ASubscriber<int>> wp = sharedSubscriber;
      sharedSubscriber->onSubscribe(
          std::make_unique<MySubscription>(std::move(wp)));
      std::cout << "AFlowable onSubscribe END" << std::endl;
    })->subscribe(std::make_unique<MySubscriber>());
  }
  std::cout << "--------- All destroyed? " << std::endl;

  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;
}
