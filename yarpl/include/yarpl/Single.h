// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Refcounted.h"

// include all the things a developer needs for using Single
#include "yarpl/single/Single.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleObservers.h"
#include "yarpl/single/SingleSubscriptions.h"
#include "yarpl/single/Singles.h"

/**
 * Create a single with code such as this:
 *
 *  auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
 *    obs->onSubscribe(SingleSubscriptions::empty());
 *    obs->onSuccess(1);
 *  });
 *
 *  // TODO add more documentation
 */
