/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_arg_metadata.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include <cinttypes>
#include <cstddef>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {

using StringMap = std::unordered_map<uint32_t, std::string>;
using InstructionsSegmentOffset = uint16_t;

struct KernelDescriptor final {
    enum AddressingMode : uint8_t {
        AddrNone,
        Stateless,
        Bindful,
        Bindless,
        BindfulAndStateless,
        BindlessAndStateless
    };

    KernelDescriptor() = default;
    ~KernelDescriptor() = default;

    struct KernelAttributes {
        KernelAttributes() { flags.packed = 0U; }

        uint32_t slmInlineSize = 0U;
        uint32_t perThreadScratchSize[2] = {0U, 0U};
        uint32_t perThreadPrivateMemorySize = 0U;
        uint32_t perThreadSystemThreadSurfaceSize = 0U;
        uint16_t requiredWorkgroupSize[3] = {0U, 0U, 0U};
        uint16_t crossThreadDataSize = 0U;
        uint16_t perThreadDataSize = 0U;
        uint16_t numArgsToPatch = 0U;
        uint16_t numGrfRequired = 0U;
        AddressingMode bufferAddressingMode = BindfulAndStateless;
        AddressingMode imageAddressingMode = Bindful;
        AddressingMode samplerAddressingMode = Bindful;

        uint8_t workgroupWalkOrder[3] = {0, 1, 2};
        uint8_t workgroupDimensionsOrder[3] = {0, 1, 2};

        uint8_t gpuPointerSize = 0;
        uint8_t simdSize = 8;
        uint8_t grfSize = 32;
        uint8_t numLocalIdChannels = 3;

        bool supportsBuffersBiggerThan4Gb() const {
            return Stateless == bufferAddressingMode;
        }

        union {
            struct {
                bool usesPrintf : 1;
                bool usesBarriers : 1;
                bool usesFencesForReadWriteImages : 1;
                bool usesFlattenedLocalIds;
                bool usesPrivateMemory : 1;
                bool usesVme : 1;
                bool usesImages : 1;
                bool usesSamplers : 1;
                bool usesDeviceSideEnqueue : 1;
                bool usesSyncBuffer : 1;
                bool useGlobalAtomics : 1;
                bool usesStatelessWrites : 1;
                bool passInlineData : 1;
                bool perThreadDataHeaderIsPresent : 1;
                bool perThreadDataUnusedGrfIsPresent : 1;
                bool requiresDisabledMidThreadPreemption : 1;
                bool requiresSubgroupIndependentForwardProgress : 1;
                bool requiresWorkgroupWalkOrder : 1;
            };
            uint32_t packed;
        } flags;
        static_assert(sizeof(KernelAttributes::flags) == sizeof(KernelAttributes::flags.packed), "");
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
            CrossThreadDataOffset privateMemorySize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset maxWorkGroupSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset simdSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset deviceSideEnqueueParentEvent = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset preferredWkgMultiple = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset localMemoryStatelessWindowSize = undefined<CrossThreadDataOffset>;
            CrossThreadDataOffset localMemoryStatelessWindowStartAddres = undefined<CrossThreadDataOffset>;
        } implicitArgs;

        std::vector<std::unique_ptr<ArgDescriptorExtended>> explicitArgsExtendedDescriptors;
    } payloadMappings;

    std::vector<ArgTypeMetadataExtended> explicitArgsExtendedMetadata;

    struct {
        std::string kernelName;
        std::string kernelLanguageAttributes;
        StringMap printfStringsMap;
        std::vector<std::pair<uint32_t, uint32_t>> deviceSideEnqueueChildrenKernelsIdOffset;
        uint32_t deviceSideEnqueueBlockInterfaceDescriptorOffset = 0U;

        struct ByValueArgument {
            ArgDescValue::Element byValueElement;
            uint16_t argNum;
        };
        StackVec<ByValueArgument, 32> allByValueKernelArguments;

        uint16_t compiledSubGroupsNumber = 0U;
        uint8_t requiredSubGroupSize = 0U;
    } kernelMetadata;

    struct {
        std::unique_ptr<DebugData> debugData;
        const void *igcInfoForGtpin = nullptr;
    } external;
};

} // namespace NEO
