/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/heap_info.h"
#include "shared/source/utilities/arrayref.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gtpin {
typedef struct igc_info_s igc_info_t;
}

namespace NEO {
struct HardwareInfo;
class BuiltinDispatchInfoBuilder;
class Device;
class Kernel;
struct KernelInfo;
class DispatchInfo;
struct KernelArgumentType;
class GraphicsAllocation;
class MemoryManager;

static const float yTilingRatioValue = 1.3862943611198906188344642429164f;

struct DeviceInfoKernelPayloadConstants {
    void *slmWindow = nullptr;
    uint32_t slmWindowSize = 0U;
    uint32_t computeUnitsUsedForScratch = 0U;
    uint32_t maxWorkGroupSize = 0U;
};

struct KernelInfo : NEO::NonCopyableAndNonMovableClass {
  public:
    KernelInfo() = default;
    ~KernelInfo();

    GraphicsAllocation *getGraphicsAllocation() const { return this->kernelAllocation; }

    const ArgDescriptor &getArgDescriptorAt(uint32_t index) const {
        DEBUG_BREAK_IF(index >= kernelDescriptor.payloadMappings.explicitArgs.size());
        return kernelDescriptor.payloadMappings.explicitArgs[index];
    }
    const StackVec<ArgDescriptor, 16> &getExplicitArgs() const {
        return kernelDescriptor.payloadMappings.explicitArgs;
    }
    const ArgTypeMetadataExtended &getExtendedMetadata(uint32_t index) const {
        DEBUG_BREAK_IF(index >= kernelDescriptor.explicitArgsExtendedMetadata.size());
        return kernelDescriptor.explicitArgsExtendedMetadata[index];
    }
    size_t getSamplerStateArrayCount() const;
    size_t getBorderColorOffset() const;
    unsigned int getMaxSimdSize() const {
        return kernelDescriptor.kernelAttributes.simdSize;
    }
    bool requiresSubgroupIndependentForwardProgress() const {
        return kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress;
    }
    size_t getMaxRequiredWorkGroupSize(size_t maxWorkGroupSize) const {
        auto requiredWorkGroupSizeX = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
        auto requiredWorkGroupSizeY = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
        auto requiredWorkGroupSizeZ = kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];
        size_t maxRequiredWorkGroupSize = static_cast<size_t>(requiredWorkGroupSizeX) * requiredWorkGroupSizeY * requiredWorkGroupSizeZ;
        if ((maxRequiredWorkGroupSize == 0) || (maxRequiredWorkGroupSize > maxWorkGroupSize)) {
            maxRequiredWorkGroupSize = maxWorkGroupSize;
        }
        return maxRequiredWorkGroupSize;
    }

    bool isVmeUsed() const {
        return kernelDescriptor.kernelAttributes.flags.usesVme;
    }

    uint32_t getConstantBufferSize() const;
    int32_t getArgNumByName(const char *name) const;

    bool createKernelAllocation(const Device &device, bool internalIsa);
    void apply(const DeviceInfoKernelPayloadConstants &constants);

    HeapInfo heapInfo = {};
    std::vector<std::pair<uint32_t, uint32_t>> childrenKernelsIdOffset;
    char *crossThreadData = nullptr;
    const BuiltinDispatchInfoBuilder *builtinDispatchBuilder = nullptr;
    uint32_t systemKernelOffset = 0;
    uint64_t kernelId = 0;
    bool isKernelHeapSubstituted = false;
    GraphicsAllocation *kernelAllocation = nullptr;
    DebugData debugData;
    bool computeMode = false;
    const gtpin::igc_info_t *igcInfoForGtpin = nullptr;

    uint64_t shaderHashCode;
    KernelDescriptor kernelDescriptor;
};

static_assert(NEO::NonCopyableAndNonMovable<KernelInfo>);

std::string concatenateKernelNames(ArrayRef<KernelInfo *> kernelInfos);

} // namespace NEO
