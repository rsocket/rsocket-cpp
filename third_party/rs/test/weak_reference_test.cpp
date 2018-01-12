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

#include <string>

#include <rs/weak_reference.h>

namespace shk {
namespace {

template <typename T>
class WithGenericConstructor {
 public:
  template <typename U>
  explicit WithGenericConstructor(U &&v)
      : t_(std::forward<U>(v)) {}

 private:
  T t_;
};

class Supertype {
 public:
  virtual ~Supertype() = default;

  virtual int GetValue() = 0;
};

class Subtype : public Supertype {
 public:
  int GetValue() override {
    return 1337;
  }
};

}  // anonymous namespace

TEST_CASE("WeakReference") {
  SECTION("WeakReferee") {
    SECTION("default constructor") {
      WeakReferee<std::string> str;
      CHECK(str == "");
    }

    SECTION("destructor") {
      SECTION("with weak reference") {
        WeakReference<std::string> ref;
        {
          WeakReferee<std::string> str = WithWeakReference(
              std::string("hey"), &ref);
        }

        CHECK(!ref);
      }

      SECTION("without weak reference") {
        WeakReference<std::string> ref;
        {
          WeakReferee<std::string> str = WithWeakReference(
              std::string("hey"), &ref);
          ref.Reset();
        }

        CHECK(!ref);
      }
    }

    SECTION("base operator=") {
      WeakReference<std::string> ref;
      WeakReferee<std::string> str = WithWeakReference(
          std::string("hey"), &ref);

      str = "new";

      CHECK(*ref == "new");
    }

    SECTION("move constructor") {
      SECTION("parameter has backref") {
        WeakReference<std::string> ref;
        WeakReferee<std::string> str = WithWeakReference(
            std::string("hey"), &ref);

        WeakReferee<std::string> moved(std::move(str));

        CHECK(str.empty());
        CHECK(*ref == "hey");
        CHECK(moved == "hey");
      }

      SECTION("parameter has no backref") {
        WeakReference<std::string> ref;
        WeakReferee<std::string> str = WithWeakReference(
            std::string("hey"), &ref);
        ref.Reset();

        WeakReferee<std::string> moved(std::move(str));

        CHECK(str.empty());
        CHECK(!ref);
        CHECK(moved == "hey");
      }

      SECTION("inner type with generic constructor") {
        WeakReference<WithGenericConstructor<int>> ref_a;
        WeakReferee<WithGenericConstructor<int>> str_a = WithWeakReference(
            WithGenericConstructor<int>(5), &ref_a);

        WeakReference<WithGenericConstructor<int>> ref_b;
        WeakReferee<WithGenericConstructor<int>> str_b = WithWeakReference(
            WithGenericConstructor<int>(6), &ref_b);

        str_a = std::move(str_b);
      }
    }

    SECTION("move assignment operator") {
      SECTION("lhs with backref, rhs with backref") {
        WeakReference<std::string> ref_a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &ref_a);

        WeakReference<std::string> ref_b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &ref_b);

        WeakReference<std::string> ref_c;
        WeakReferee<std::string> str_c = WithWeakReference(
            std::string("str_c"), &ref_c);

        str_a = std::move(str_b);

        str_c = std::move(str_b);  // str_b should not have a backref to ref_b

        CHECK(str_a == "str_b");
        CHECK(str_b.empty());
        CHECK(!ref_a);
        CHECK(*ref_b == "str_b");
      }

      SECTION("lhs without backref, rhs with backref") {
        WeakReference<std::string> ref_a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &ref_a);
        ref_a.Reset();

        WeakReference<std::string> ref_b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &ref_b);

        str_a = std::move(str_b);

        CHECK(str_a == "str_b");
        CHECK(str_b.empty());
        CHECK(!ref_a);
        CHECK(*ref_b == "str_b");
      }

      SECTION("lhs with backref, rhs without backref") {
        WeakReference<std::string> ref_a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &ref_a);

        WeakReference<std::string> ref_b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &ref_b);
        ref_b.Reset();

        str_a = std::move(str_b);

        CHECK(str_a == "str_b");
        CHECK(str_b.empty());
        CHECK(!ref_a);
        CHECK(!ref_b);
      }
    }
  }

  SECTION("WeakReference") {
    SECTION("default constructor") {
      WeakReference<std::string> backref;
      CHECK(!backref);
    }

    SECTION("destructor") {
      SECTION("with weak referee") {
        WeakReference<std::string> ref_a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &ref_a);

        WeakReference<std::string> ref_b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &ref_b);

        std::make_unique<WeakReference<std::string>>(std::move(ref_a));

        // Now, str_a should have no weak reference pointer. If it does, it will
        // point to freed memory, which asan will catch here:
        str_b = std::move(str_a);

        CHECK(!ref_a);
        CHECK(!ref_b);
      }

      SECTION("without weak reference") {
        WeakReference<std::string> ref;
      }
    }

    SECTION("move constructor") {
      SECTION("empty parameter") {
        WeakReference<std::string> a;
        WeakReference<std::string> b(std::move(a));
        CHECK(!a);
        CHECK(!b);
      }

      SECTION("nonempty parameter") {
        WeakReference<std::string> a;
        WeakReferee<std::string> str = WithWeakReference(
            std::string("hey"), &a);
        WeakReference<std::string> b(std::move(a));

        CHECK(!a);
        CHECK(*b == "hey");

        WeakReferee<std::string> moved_str(std::move(str));
        CHECK(*b == "hey");
      }
    }

    SECTION("move assignment operator") {
      SECTION("empty lhs and rhs") {
        WeakReference<std::string> a;
        WeakReference<std::string> b;

        b = std::move(a);

        CHECK(!a);
        CHECK(!b);
      }

      SECTION("empty lhs, nonempty rhs") {
        WeakReference<std::string> a;
        WeakReferee<std::string> str = WithWeakReference(
            std::string("hey"), &a);
        WeakReference<std::string> b;

        b = std::move(a);

        CHECK(!a);
        CHECK(*b == "hey");

        WeakReferee<std::string> moved_str(std::move(str));
        CHECK(*b == "hey");
      }

      SECTION("nonempty lhs, nonempty rhs") {
        WeakReference<std::string> a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &a);

        WeakReference<std::string> b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &b);

        WeakReference<std::string> c;
        WeakReferee<std::string> str_c = WithWeakReference(
            std::string("str_c"), &c);

        b = std::move(a);

        CHECK(!a);
        CHECK(*b == "str_a");

        str_b = std::move(str_c);
        CHECK(*b == "str_a");
      }
    }

    SECTION("Reset") {
      SECTION("nonempty") {
        WeakReference<std::string> a;
        WeakReferee<std::string> str_a = WithWeakReference(
            std::string("str_a"), &a);

        WeakReference<std::string> b;
        WeakReferee<std::string> str_b = WithWeakReference(
            std::string("str_b"), &b);

        a.Reset();
        CHECK(!a);

        str_b = std::move(str_a);

        CHECK(!a);
      }

      SECTION("empty") {
        WeakReference<std::string> a;
        a.Reset();
      }
    }

    SECTION("operator bool") {
      // Already tested by the other unit tests
    }

    SECTION("operator*") {
      WeakReference<std::string> a;
      WeakReferee<std::string> str_a = WithWeakReference(
          std::string("str_a"), &a);

      *a = "new";  // non-const
      const auto &const_a = a;
      CHECK(*const_a == "new");  // const
    }

    SECTION("operator->") {
      WeakReference<std::string> a;
      WeakReferee<std::string> str_a = WithWeakReference(
          std::string("str_a"), &a);

      a->append("_hey");  // non-const
      const auto &const_a = a;
      CHECK(const_a->find("_hey") == 5);  // const
    }
  }

  SECTION("variadric WithWeakReference function") {
    SECTION("no weak references") {
      std::string value = "hello there!";
      auto value_weak_referee = WithWeakReference(value);
      CHECK(value == value_weak_referee);

      static_assert(
          std::is_same<std::string, decltype(value_weak_referee)>::value,
          "WithWeakReference with no weak reference should not wrap");
    }

    SECTION("multiple weak references") {
      WeakReference<std::string> ref_a;
      WeakReference<std::string> ref_b;
      auto str = WithWeakReference(std::string("str"), &ref_a, &ref_b);

      CHECK(str == "str");
      CHECK(*ref_a == "str");
      CHECK(*ref_b == "str");

      auto str_2 = std::move(str);

      CHECK(str_2 == "str");
      CHECK(*ref_a == "str");
      CHECK(*ref_b == "str");
    }
  }

  SECTION("WeakReference to supertype of WeakReferee") {
    WeakReference<Supertype> a_ref;
    WeakReferee<Subtype> a = WithWeakReference(
        Subtype(), &a_ref);

    CHECK(a.GetValue() == 1337);
    CHECK(a_ref->GetValue() == 1337);
  }

  SECTION("moving WeakReferee and WeakReference together") {
    SECTION("reference first") {
      struct Together {
        Together(const std::string &str)
            : str(WithWeakReference(str, &ref)) {}

        WeakReference<std::string> ref;
        WeakReferee<std::string> str;
      };

      Together a("a");
      Together b("b");

      CHECK(*a.ref == "a");
      CHECK(a.str == "a");
      CHECK(*b.ref == "b");
      CHECK(b.str == "b");

      a = std::move(b);
      CHECK(!b.ref);
      CHECK(b.str == "");
      CHECK(*a.ref == "b");
      CHECK(a.str == "b");
    }

    SECTION("referee first") {
      static WeakReference<std::string> tmp;

      struct Together {
        Together(const std::string &s)
            : str(WithWeakReference(std::string("tmp"), &tmp)) {
          str = WithWeakReference(s, &ref);
        }

        WeakReferee<std::string> str;
        WeakReference<std::string> ref;
      };

      Together a("a");
      Together b("b");

      CHECK(*a.ref == "a");
      CHECK(a.str == "a");
      CHECK(*b.ref == "b");
      CHECK(b.str == "b");

      a = std::move(b);
      CHECK(!b.ref);
      CHECK(b.str == "");
      CHECK(*a.ref == "b");
      CHECK(a.str == "b");
    }
  }
}

}  // namespace shk
