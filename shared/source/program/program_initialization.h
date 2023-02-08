/*
 * Copyright (C) 2020-2023 Intel Corporation
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

GraphicsAllocation *allocateGlobalsSurface(SVMAllocsManager *const svmAllocManager, Device &device,
                                           size_t totalSize, size_t zeroInitSize, bool constant,
                                           LinkerInput *const linkerInput, const void *initData);

} // namespace NEO
