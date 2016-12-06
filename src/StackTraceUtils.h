// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>

namespace reactivesocket {

std::string getStackTrace();

#ifndef REACTIVE_SOCKET_EXTERNAL_STACK_TRACE_UTILS
inline std::string getStackTrace() {return "";}
#endif

} // reactivesocket
