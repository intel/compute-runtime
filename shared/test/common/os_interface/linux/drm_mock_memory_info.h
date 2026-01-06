/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include <vector>

const std::vector<NEO::MemoryRegion> memoryRegions = {
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 64 * MemoryConstants::gigaByte, 0},
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 8 * MemoryConstants::gigaByte, 0}};

struct MockMemoryInfo : public NEO::MemoryInfo {
    using NEO::MemoryInfo::localMemoryRegions;
    using NEO::MemoryInfo::smallBarDetected;

    MockMemoryInfo(const NEO::Drm &drm) : MemoryInfo(memoryRegions, drm) {}
};

const std::vector<NEO::MemoryRegion> extendedMemoryRegions = {
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1}, 64 * MemoryConstants::gigaByte, 0},
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0x100}, 8 * MemoryConstants::gigaByte, 0},
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0x200}, 8 * MemoryConstants::gigaByte, 0},
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0x400}, 8 * MemoryConstants::gigaByte, 0},
    {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0x800}, 8 * MemoryConstants::gigaByte, 0}};

struct MockExtendedMemoryInfo : public NEO::MemoryInfo {
    MockExtendedMemoryInfo(const NEO::Drm &drm) : MemoryInfo(extendedMemoryRegions, drm) {}
};
