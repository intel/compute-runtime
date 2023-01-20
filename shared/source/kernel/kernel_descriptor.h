/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_arg_metadata.h"

#include <array>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {

using StringMap = std::unordered_map<uint32_t, std::string>;
using InstructionsSegmentOffset = uint16_t;

struct KernelDescriptor {
    enum AddressingMode : uint8_t {
        AddrNone,
        Stateless,
        Bindful,
        Bindless,
        BindfulAndStateless,
        BindlessAndStateless
    };

    KernelDescriptor() = default;
    virtual ~KernelDescriptor() = default;

    void updateCrossThreadDataSize();

    struct KernelAttributes {
        uint32_t slmInlineSize = 0U;
        uint32_t perThreadScratchSize[2] = {0U, 0U};
        uint32_t perHwThreadPrivateMemorySize = 0U;
        uint32_t perThreadSystemThreadSurfaceSize = 0U;
        uint32_t numThreadsRequired = 0u;
        ThreadArbitrationPolicy threadArbitrationPolicy = NotPresent;
        uint16_t requiredWorkgroupSize[3] = {0U, 0U, 0U};
        uint16_t crossThreadDataSize = 0U;
        uint16_t inlineDataPayloadSize = 0U;
        uint16_t perThreadDataSize = 0U;
        uint16_t numArgsToPatch = 0U;
        uint16_t numGrfRequired = GrfConfig::DefaultGrfNumber;
        uint8_t barrierCount = 0u;
        bool hasNonKernelArgLoad = true;
        bool hasNonKernelArgStore = true;
        bool hasNonKernelArgAtomic = true;
        bool hasIndirectStatelessAccess = false;

        AddressingMode bufferAddressingMode = BindfulAndStateless;
        AddressingMode imageAddressingMode = Bindful;
        AddressingMode samplerAddressingMode = Bindful;

        DeviceBinaryFormat binaryFormat = DeviceBinaryFormat::Unknown;

        uint8_t workgroupWalkOrder[3] = {0, 1, 2};
        uint8_t workgroupDimensionsOrder[3] = {0, 1, 2};

        uint8_t gpuPointerSize = 0;
        uint8_t simdSize = 8;
        uint8_t numLocalIdChannels = 0;
        uint8_t localId[3] = {0U, 0U, 0U};

        bool supportsBuffersBiggerThan4Gb() const {
            return Stateless == bufferAddressingMode;
        }

        bool usesBarriers() const {
            return 0 != barrierCount;
        }

        union {
            struct {
                // 0
                bool usesSystolicPipelineSelectMode : 1;
                bool usesStringMapForPrintf : 1;
                bool usesPrintf : 1;
                bool usesFencesForReadWriteImages : 1;
                bool usesFlattenedLocalIds : 1;
                bool usesPrivateMemory : 1;
                bool usesVme : 1;
                bool usesImages : 1;
                // 1
                bool usesSamplers : 1;
                bool usesSyncBuffer : 1;
                bool useGlobalAtomics : 1;
                bool usesStatelessWrites : 1;
                bool passInlineData : 1;
                bool perThreadDataHeaderIsPresent : 1;
                bool perThreadDataUnusedGrfIsPresent : 1;
                bool requiresDisabledEUFusion : 1;
                // 2
                bool requiresDisabledMidThreadPreemption : 1;
                bool requiresSubgroupIndependentForwardProgress : 1;
                bool requiresWorkgroupWalkOrder : 1;
                bool requiresImplicitArgs : 1;
                bool useStackCalls : 1;
                bool hasRTCalls : 1;
                bool isInvalid : 1;
                bool hasSample : 1;
            };
            std::array<bool, 3> packed;
        } flags = {};
        static_assert(sizeof(KernelAttributes::flags) == sizeof(KernelAttributes::flags.packed), "");

        bool usesStringMap() const {
            if (binaryFormat == DeviceBinaryFormat::Patchtokens) {
                return flags.usesStringMapForPrintf || flags.requiresImplicitArgs;
            }
            return false;
        }
    } kernelAttributes;

    struct {
        InstructionsSegmentOffset skipPerThreadDataLoad = 0U;
        InstructionsSegmentOffset skipSetFFIDGP = 0U;
        InstructionsSegmentOffset systemKernel = 0U;
    } entryPoints;

    struct PayloadMappings {
        struct {
            CrossThreadDataOffset globalWorkOffset[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
            CrossThreadDataOffset globalWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
            CrossThreadDataOffset localWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
            CrossThreadDataOffset localWorkSize2[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};

            CrossThreadDataOffset enqueuedLocalWorkSize[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
            CrossThreadDataOffset numWorkGroups[3] = {undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>, undefined<CrossThreadDataOffset>};
            CrossThreadDataOffset workDim = undefined<CrossThreadDataOffset>;
        } dispatchTraits;

        struct {
            SurfaceStateHeapOffset tableOffset = undefined<SurfaceStateHeapOffset>;
            uint8_t numEntries = 0;
        } bindingTable;

        struct {
            DynamicStateHeapOffset tableOffset = undefined<DynamicStateHeapOffset>;
            DynamicStateHeapOffset borderColor = undefined<DynamicStateHeapOffset>;
            uint8_t numSamplers = 0;
        } samplerTable;

        StackVec<ArgDescriptor, 16> explicitArgs;

        struct {
            ArgDescPointer printfSurfaceAddress;
            ArgDescPointer globalVariablesSurfaceAddress;
            ArgDescPointer globalConstantsSurfaceAddress;
            ArgDescPointer privateMemoryAddress;
            ArgDescPointer deviceSideEnqueueEventPoolSurfaceAddress;
            ArgDescPointer deviceSideEnqueueDefaultQueueSurfaceAddress;
            ArgDescPointer systemThreadSurfaceAddress;
            ArgDescPointer syncBufferAddress;
            ArgDescPointer rtDispatchGlobals;
            CrossThreadDataOffset privateMemorySize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset maxWorkGroupSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset simdSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset deviceSideEnqueueParentEvent = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset preferredWkgMultiple = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset localMemoryStatelessWindowSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset localMemoryStatelessWindowStartAddres = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset implicitArgsBuffer = undefined<CrossThreadDataOffset>;
        } implicitArgs;

        std::vector<std::unique_ptr<ArgDescriptorExtended>> explicitArgsExtendedDescriptors;
    } payloadMappings;

    std::vector<ArgTypeMetadataExtended> explicitArgsExtendedMetadata;

    struct InlineSampler {
        enum class AddrMode : uint8_t {
            None,
            Repeat,
            ClampEdge,
            ClampBorder,
            Mirror
        };
        enum class FilterMode : uint8_t {
            Nearest,
            Linear
        };
        static constexpr size_t borderColorStateSize = 64U;
        static constexpr size_t samplerStateSize = 16U;

        uint32_t samplerIndex;
        bool isNormalized;
        AddrMode addrMode;
        FilterMode filterMode;
        constexpr uint32_t getSamplerBindfulOffset() const {
            return borderColorStateSize + samplerStateSize * samplerIndex;
        }
    };
    std::vector<InlineSampler> inlineSamplers;

    struct {
        std::string kernelName;
        std::string kernelLanguageAttributes;
        StringMap printfStringsMap;

        uint16_t compiledSubGroupsNumber = 0U;
        uint8_t requiredSubGroupSize = 0U;
    } kernelMetadata;

    struct {
        std::unique_ptr<DebugData> debugData;
        std::unique_ptr<uint8_t[]> relocatedDebugData;
        const void *igcInfoForGtpin = nullptr;
    } external;

    std::vector<uint8_t> generatedSsh;
    std::vector<uint8_t> generatedDsh;
};

} // namespace NEO
