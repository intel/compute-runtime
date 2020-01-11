/*
 * Copyright (C) 2020 Intel Corporation
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
                                           size_t size, bool constant,
                                           LinkerInput *const linkerInput, const void *const initData);

} // namespace NEO
