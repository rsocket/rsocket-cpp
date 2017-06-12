// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>

namespace yarpl {

inline const char* exceptionStr(std::exception_ptr ep) {
  try {
    std::rethrow_exception(ep);
  } catch (const std::exception& e) {
    return e.what();
  } catch (...) {
    return "<unknown exception>";
  }
}
}
