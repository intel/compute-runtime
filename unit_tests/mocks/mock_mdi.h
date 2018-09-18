/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/dispatch_info.h"

using namespace OCLRT;

class MockMultiDispatchInfo : public MultiDispatchInfo {
  public:
    MockMultiDispatchInfo(Kernel *kernel) : MultiDispatchInfo(kernel) {
        DispatchInfo di(kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0});
        dispatchInfos.push_back(di);
    }
    MockMultiDispatchInfo(std::vector<Kernel *> kernels) {
        for (auto kernel : kernels) {
            DispatchInfo di(kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0});
            dispatchInfos.push_back(di);
        }
    }
    MockMultiDispatchInfo(std::vector<DispatchInfo *> dis) {
        for (auto di : dis) {
            dispatchInfos.push_back(*di);
        }
    }
};
