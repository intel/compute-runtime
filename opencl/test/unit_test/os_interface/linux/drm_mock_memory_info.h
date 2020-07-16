/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/memory_info_impl.h"

constexpr drm_i915_memory_region_info memoryRegions[2] = {
    {{I915_MEMORY_CLASS_SYSTEM, 0}, 0, 0, 64 * GB, 0, {}},
    {{I915_MEMORY_CLASS_DEVICE, 0}, 0, 0, 8 * GB, 0, {}}};

struct MockMemoryInfo : public MemoryInfoImpl {
    MockMemoryInfo() : MemoryInfoImpl(memoryRegions, 2) {}
    ~MockMemoryInfo() override{};
};
