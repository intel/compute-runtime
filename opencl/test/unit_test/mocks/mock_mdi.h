/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/helpers/dispatch_info.h"

using namespace NEO;

class MockMultiDispatchInfo : public MultiDispatchInfo {
  public:
    using MultiDispatchInfo::dispatchInfos;

    MockMultiDispatchInfo(ClDevice *clDevice, Kernel *kernel) : MultiDispatchInfo(kernel) {
        DispatchInfo di(clDevice, kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0});
        di.setNumberOfWorkgroups({10, 1, 1});
        di.setTotalNumberOfWorkgroups({10, 1, 1});
        dispatchInfos.push_back(di);
    }
    MockMultiDispatchInfo(ClDevice *clDevice, std::vector<Kernel *> kernels) {
        for (auto kernel : kernels) {
            DispatchInfo di(clDevice, kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0});
            di.setNumberOfWorkgroups({10, 1, 1});
            di.setTotalNumberOfWorkgroups({10, 1, 1});
            dispatchInfos.push_back(di);
        }
    }
    MockMultiDispatchInfo(std::vector<DispatchInfo *> dis) {
        for (auto di : dis) {
            dispatchInfos.push_back(*di);
        }
    }
};

void cloneMdi(MultiDispatchInfo &dst, const MultiDispatchInfo &src);