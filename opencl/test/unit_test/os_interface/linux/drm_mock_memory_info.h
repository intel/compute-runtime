/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/memory_info.h"

const std::vector<MemoryRegion> memoryRegions = {
    {{I915_MEMORY_CLASS_SYSTEM, 0}, 64 * GB, 0},
    {{I915_MEMORY_CLASS_DEVICE, 0}, 8 * GB, 0}};

struct MockMemoryInfo : public MemoryInfo {
    MockMemoryInfo() : MemoryInfo(memoryRegions) {}
    ~MockMemoryInfo() override = default;
};

const std::vector<MemoryRegion> extendedMemoryRegions = {
    {{I915_MEMORY_CLASS_SYSTEM, 1}, 64 * GB, 0},
    {{I915_MEMORY_CLASS_DEVICE, 0x100}, 8 * GB, 0},
    {{I915_MEMORY_CLASS_DEVICE, 0x200}, 8 * GB, 0},
    {{I915_MEMORY_CLASS_DEVICE, 0x400}, 8 * GB, 0},
    {{I915_MEMORY_CLASS_DEVICE, 0x800}, 8 * GB, 0}};

struct MockExtendedMemoryInfo : public MemoryInfo {
    MockExtendedMemoryInfo() : MemoryInfo(extendedMemoryRegions) {}
    ~MockExtendedMemoryInfo() override = default;
};
