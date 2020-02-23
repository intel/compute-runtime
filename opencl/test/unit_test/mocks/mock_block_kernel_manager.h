/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/program/block_kernel_manager.h"

namespace NEO {

class MockBlockKernelManager : public BlockKernelManager {
  public:
    MockBlockKernelManager() = default;
    ~MockBlockKernelManager() = default;
    using BlockKernelManager::blockKernelInfoArray;
    using BlockKernelManager::blockPrivateSurfaceArray;
};
} // namespace NEO
