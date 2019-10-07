/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <chrono>
#include <iostream>
#include <utility>

namespace NEO {
class TimeMeasureWrapper {
  public:
    template <class O, class F, class... Args>
    static decltype(auto) functionExecution(O &&obj, F &&func, Args &&... args) {
        auto start = std::chrono::system_clock::now();
        auto retVal = (obj.*func)(std::forward<Args>(args)...);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsedTime = end - start;
        std::cout << "Elapsed time: " << elapsedTime.count() << "\n";
        return retVal;
    }
};
}; // namespace NEO
