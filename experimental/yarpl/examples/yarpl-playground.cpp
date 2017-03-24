// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include "FlowableAExamples.h"
#include "FlowableBExamples.h"
#include "FlowableCExamples.h"
#include "FlowableExamples.h"
#include "ObservableExamples.h"

int main() {
  std::cout << "*** Run ObservableExamples ***" << std::endl;
  ObservableExamples::run();
  std::cout << "*** Run FlowableExamples ***" << std::endl;
  FlowableExamples::run();
  std::cout << "*** Run FlowableExamplesA ***" << std::endl;
  FlowableAExamples::run();
  std::cout << "*** Run FlowableExamplesB ***" << std::endl;
  FlowableBExamples::run();
  std::cout << "*** Run FlowableExamplesC ***" << std::endl;
  FlowableCExamples::run();
}
