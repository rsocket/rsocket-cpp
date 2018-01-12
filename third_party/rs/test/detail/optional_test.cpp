// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <catch.hpp>

#include <rs/detail/optional.h>

namespace shk {
namespace detail {
namespace {

class RefCounter {
 public:
  RefCounter() : _counter(NULL) {}

  explicit RefCounter(int* counter)
      : _counter(counter) {
    inc();
  }

  virtual ~RefCounter() { dec(); }

  RefCounter(const RefCounter& rhs)
      : _counter(rhs._counter) {
    inc();
  }

  RefCounter(RefCounter&& rhs) : _counter(NULL) {
    std::swap(_counter, rhs._counter);
  }

  RefCounter& operator=(const RefCounter& rhs) {
    dec();
    _counter = rhs._counter;
    inc();

    return *this;
  }

  RefCounter& operator=(RefCounter&& rhs) {
    if (this != &rhs) {
      _counter = rhs._counter;
      rhs._counter = NULL;
    }
    return *this;
  }

 private:
  int *_counter;

  void inc() {
    if (_counter) {
      (*_counter)++;
    }
  }

  void dec() {
    if (_counter) {
      (*_counter)--;
    }
  }
};

template<typename T>
class Holder {
 public:
  explicit Holder(const T &val) : _val(val) {}
  virtual ~Holder() {}

  Holder(const Holder<T>&) = delete;
  Holder<T>& operator=(const Holder<T>&) = delete;

  T get() { return _val; }
  const T get() const { return _val; }
  void set(const T& val) { _val = val; }

 private:
  T _val;
};

}  // anonymous namespace

TEST_CASE("Optional") {
  SECTION("Harness") {
    SECTION("RefCounter") {
      int counter = 0;
      {
        RefCounter rc1(&counter);
        CHECK(1 == counter);
        RefCounter rc2(rc1);
        CHECK(2 == counter);
        RefCounter rc3;
        rc3 = rc2;
        CHECK(3 == counter);
        rc1 = rc3;
        CHECK(3 == counter);
        RefCounter rc4;
      }
      CHECK(0 == counter);
    }

    SECTION("Holder") {
      Holder<int> h(0);
      CHECK(0 == h.get());
      h.set(1);
      CHECK(1 == h.get());
    }
  }

  SECTION("Construct") {
    int requested;
    auto req = [&requested]() {};  // req is copyable but not assignable

    SECTION("MoveConstructNonAssignable") {
      detail::Optional<std::tuple<decltype(req)>> opt;
      auto opt2 = std::move(opt);
    }

    SECTION("CopyConstructNonAssignable") {
      detail::Optional<std::tuple<decltype(req)>> opt;
      auto opt2 = opt;
    }

    SECTION("CopyConstructNonAssignableByValue") {
      std::tuple<decltype(req)> tuple(req);
      detail::Optional<std::tuple<decltype(req)>> opt(tuple);
    }

    SECTION("MoveConstructNonAssignableByValue") {
      detail::Optional<std::tuple<decltype(req)>> opt((
          std::tuple<decltype(req)>(req)));
    }
  }

  SECTION("IsSet") {
    SECTION("Uninitialized") {
      Optional<int> m;
      CHECK(!m);
      CHECK(!m.IsSet());
    }

    SECTION("Initialized") {
      Optional<int> m(0);
      CHECK(!!m);
      CHECK(m.IsSet());
    }
  }

  SECTION("Assignment") {
    SECTION("Assignment") {
      int counter = 0;
      {
        RefCounter rc(&counter);
        Optional<RefCounter> m1;
        CHECK(1 == counter);
        m1 = rc;
        CHECK(2 == counter);
        Optional<RefCounter> m2;
        m2 = m1;
        CHECK(3 == counter);
        m1 = m2;
        CHECK(3 == counter);
      }
      CHECK(0 == counter);
    }

    SECTION("MoveAssignment") {
      int counter = 0;
      {
        RefCounter rc(&counter);
        Optional<RefCounter> m1;
        CHECK(1 == counter);
        m1 = std::move(rc);
        CHECK(1 == counter);
        Optional<RefCounter> m2;
        m2 = std::move(m1);
        CHECK(1 == counter);
      }
      CHECK(0 == counter);
    }
  }

  SECTION("Constructor") {
    SECTION("CopyConstructor") {
      int counter = 0;
      {
        RefCounter rc(&counter);
        Optional<RefCounter> m1(rc);
        CHECK(2 == counter);
        Optional<RefCounter> m2(m1);
        CHECK(3 == counter);
      }
      CHECK(0 == counter);
    }

    SECTION("MoveConstructor") {
      int counter = 0;
      {
        RefCounter rc(&counter);
        Optional<RefCounter> m1(std::move(rc));
        CHECK(1 == counter);
        Optional<RefCounter> m2(std::move(m1));
        CHECK(1 == counter);
      }
      CHECK(0 == counter);
    }
  }

  SECTION("Clear") {
    SECTION("Uninitialized") {
      Optional<int> m;
      CHECK(!m.IsSet());
      m.Clear();
      CHECK(!m.IsSet());
    }

    SECTION("Initialized") {
      Optional<int> m(0);
      CHECK(m.IsSet());
      m.Clear();
      CHECK(!m.IsSet());
    }
  }

  SECTION("Swap") {
    SECTION("Uninitialized") {
      Optional<int> m1;
      Optional<int> m2;
      std::swap(m1, m2);
      CHECK(!m1.IsSet());
      CHECK(!m2.IsSet());
    }

    SECTION("FirstInitialized") {
      Optional<int> m1(1);
      Optional<int> m2;
      std::swap(m1, m2);
      CHECK(!m1.IsSet());
      CHECK(1 == m2);
    }

    SECTION("SecondInitialized") {
      Optional<int> m1;
      Optional<int> m2(1);
      std::swap(m1, m2);
      CHECK(1 == m1);
      CHECK(!m2.IsSet());
    }

    SECTION("BothInitialized") {
      Optional<int> m1(1);
      Optional<int> m2(2);
      std::swap(m1, m2);
      CHECK(2 == m1);
      CHECK(1 == m2);
    }
  }

  SECTION("Reference") {
    SECTION("Construct") {
      Optional<int&> m;
    }

    SECTION("Equals") {
      int val = 1;
      int& ref = val;
      Optional<int&> m(ref);
      CHECK(1 == *m);
    }

    SECTION("Compare") {
      int val1 = 1;
      int& ref1 = val1;
      Optional<int&> m1(ref1);

      int val2 = 2;
      int& ref2 = val2;
      Optional<int&> m2(ref2);

      int val3 = 2;
      int& ref3 = val3;
      Optional<int&> m3(ref3);

      CHECK(ref1 == m1);
      CHECK(m1 == ref1);
      CHECK(m2 == m3);
      CHECK(!(ref1 != m1));
      CHECK(!(m1 != ref1));
      CHECK(!(m2 != m3));

      CHECK(1 == m1);
      CHECK(m1 == 1);
      CHECK(!(1 != m1));
      CHECK(!(m1 != 1));

      CHECK(m1 < m2);
      CHECK(m1 < ref2);
      CHECK(ref1 < m2);
      CHECK(!(m1 > m2));
      CHECK(!(m1 > ref2));
      CHECK(!(ref1 > m2));

      CHECK(1 < m2);
      CHECK(m1 < 2);
      CHECK(!(1 > m2));
      CHECK(!(m1 > 2));

      CHECK(m1 <= m1);
      CHECK(m1 <= ref1);
      CHECK(ref1 <= m1);
      CHECK(m1 >= m1);
      CHECK(m1 >= ref1);
      CHECK(ref1 >= m1);
    }

    SECTION("Copy") {
      Holder<int> val(1);
      auto& ref = val;
      Optional<decltype(ref)> m(ref);

      CHECK(1 == m->get());
      m->set(2);
      CHECK(2 == m->get());
      val.set(3);
      CHECK(3 == m->get());
    }

    SECTION("MoveConstruct") {
      Holder<int> val(1);
      auto& ref = val;
      Optional<decltype(ref)> m(std::move(ref));

      CHECK(1 == m->get());
      m->set(2);
      CHECK(2 == m->get());
      val.set(3);
      CHECK(3 == m->get());
    }
  }

  SECTION("Functional") {
    SECTION("UninitializedMap") {
      Optional<int> m1;
      Optional<int> m2 = m1.Map([] (int a) -> int { return a; });
      CHECK(!m2);
    }

    SECTION("InitializedMap") {
      Optional<int> m1(1);
      Optional<int> m2 = m1.Map([] (int a) -> int { return a; });
      CHECK(1 == m1);
    }

    SECTION("UninitializedIfElse") {
      Optional<int> m1;
      int m2 = m1.IfElse([] (int a) -> int {
        return 0;
      }, [] () -> int {
        return 1;
      });
      CHECK(1 == m2);
    }

    SECTION("InitializedIfElse") {
      Optional<int> m1(1);
      int m2 = m1.IfElse([] (int a) -> int {
        return 0;
      }, [] () -> int {
        return 1;
      });
      CHECK(0 == m2);
    }

    SECTION("InitializedIfElseInvocation") {
      bool flag1 = false;
      bool flag2 = false;
      Optional<int> m1(1);
      m1.IfElse([&] (int a) {
        flag1 = true;
      }, [&] () {
        flag2 = true;
      });
      CHECK(flag1);
      CHECK(!flag2);
    }

    SECTION("UninitializedIfElseInvocation") {
      bool flag1 = false;
      bool flag2 = false;
      Optional<int> m1;
      m1.IfElse([&] (int a) {
        flag1 = true;
      }, [&] () {
        flag2 = true;
      });
      CHECK(!flag1);
      CHECK(flag2);
    }

    SECTION("UninitializedEach") {
      bool flag = false;
      Optional<int> m1;
      m1.Each([&] (int a) {
        flag = true;
      });
      CHECK(!flag);
    }

    SECTION("InitializedEach") {
      bool flag = false;
      Optional<int> m1(1);
      m1.Each([&] (int a) {
        flag = true;
      });
      CHECK(flag);
    }

    SECTION("UninitializedConstEach") {
      bool flag = false;
      const Optional<int> m1;
      m1.Each([&] (int a) {
        flag = true;
      });
      CHECK(!flag);
    }

    SECTION("InitializedConstEach") {
      bool flag = false;
      const Optional<int> m1(1);
      m1.Each([&] (int a) {
        flag = true;
      });
      CHECK(flag);
    }
  }

  SECTION("Equal") {
    Optional<int> empty;
    Optional<int> one(1);
    Optional<int> two(2);
    Optional<int> three(3);

    CHECK(empty == empty);
    CHECK(one == one);
    CHECK(!(one == empty));
    CHECK(!(empty == one));
    CHECK(!(one == two));
    CHECK(!(two == one));

    CHECK(one == 1);
    CHECK(1 == one);
    CHECK(1 != empty);
    CHECK(empty != 1);

    CHECK(!(empty != empty));
    CHECK(!(one != one));
    CHECK(one != empty);
    CHECK(empty != one);
    CHECK(one != two);
    CHECK(two != one);
  }

  SECTION("Compare") {
    Optional<int> empty;
    Optional<int> one(1);
    Optional<int> two(2);

    CHECK(empty < one);
    CHECK(one > empty);

    CHECK(one < two);
    CHECK(two > one);
    CHECK(!(one > two));
    CHECK(!(two < one));

    CHECK(one <= one);
    CHECK(one >= one);
    CHECK(one <= two);
    CHECK(two >= one);
    CHECK(!(two <= one));
    CHECK(!(one >= two));

    CHECK(1 < two);
    CHECK(two > 1);
    CHECK(empty < 1);
    CHECK(1 > empty);
  }
}

}  // namespace detail
}  // namespace shk
