// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace yarpl {

/**
 * Represents a disposable resource.
 */
class Disposable {
 public:
  Disposable() {}
  virtual ~Disposable() = default;
  Disposable(Disposable&&) = delete;
  Disposable(const Disposable&) = delete;
  Disposable& operator=(Disposable&&) = delete;
  Disposable& operator=(const Disposable&) = delete;

  /**
   * Dispose the resource, the operation should be idempotent.
   */
  virtual void dispose() = 0;

  /**
   * Returns true if this resource has been disposed.
   * @return true if this resource has been disposed
   */
  virtual bool isDisposed() = 0;
};
}
