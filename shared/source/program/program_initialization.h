/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

namespace NEO {

class Device;
class GraphicsAllocation;
class SVMAllocsManager;
struct LinkerInput;
class SharedPoolAllocation;

[[nodiscard]] SharedPoolAllocation *allocateGlobalsSurface(SVMAllocsManager *const svmAllocManager, Device &device,
                                                           size_t totalSize, size_t zeroInitSize, bool constant,
                                                           LinkerInput *const linkerInput, const void *initData);

} // namespace NEO
