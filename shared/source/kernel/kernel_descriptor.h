/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_arg_metadata.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <cinttypes>
#include <cstddef>
#include <limits>
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

    void updateCrossThreadDataSize() {
        uint32_t crossThreadDataSize = 0;
        for (uint32_t i = 0; i < 3; i++) {
            if (isValidOffset(payloadMappings.dispatchTraits.globalWorkOffset[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.globalWorkOffset[i] + sizeof(uint32_t));
            }
            if (isValidOffset(payloadMappings.dispatchTraits.globalWorkSize[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.globalWorkSize[i] + sizeof(uint32_t));
            }
            if (isValidOffset(payloadMappings.dispatchTraits.localWorkSize[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.localWorkSize[i] + sizeof(uint32_t));
            }
            if (isValidOffset(payloadMappings.dispatchTraits.localWorkSize2[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.localWorkSize2[i] + sizeof(uint32_t));
            }
            if (isValidOffset(payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i] + sizeof(uint32_t));
            }
            if (isValidOffset(payloadMappings.dispatchTraits.numWorkGroups[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.numWorkGroups[i] + sizeof(uint32_t));
            }
        }

        if (isValidOffset(payloadMappings.dispatchTraits.workDim)) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.workDim + sizeof(uint32_t));
        }

        StackVec<ArgDescPointer *, 8> implicitArgsVec({&payloadMappings.implicitArgs.printfSurfaceAddress,
                                                       &payloadMappings.implicitArgs.globalVariablesSurfaceAddress,
                                                       &payloadMappings.implicitArgs.globalConstantsSurfaceAddress,
                                                       &payloadMappings.implicitArgs.privateMemoryAddress,
                                                       &payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress,
                                                       &payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress,
                                                       &payloadMappings.implicitArgs.systemThreadSurfaceAddress,
                                                       &payloadMappings.implicitArgs.syncBufferAddress});

        for (size_t i = 0; i < implicitArgsVec.size(); i++) {
            if (isValidOffset(implicitArgsVec[i]->bindless)) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, implicitArgsVec[i]->bindless + sizeof(uint32_t));
            }

            if (isValidOffset(implicitArgsVec[i]->stateless)) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, implicitArgsVec[i]->stateless + implicitArgsVec[i]->pointerSize);
            }
        }

        StackVec<CrossThreadDataOffset *, 7> implicitArgsVec2({&payloadMappings.implicitArgs.privateMemorySize,
                                                               &payloadMappings.implicitArgs.maxWorkGroupSize,
                                                               &payloadMappings.implicitArgs.simdSize,
                                                               &payloadMappings.implicitArgs.deviceSideEnqueueParentEvent,
                                                               &payloadMappings.implicitArgs.preferredWkgMultiple,
                                                               &payloadMappings.implicitArgs.localMemoryStatelessWindowSize,
                                                               &payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres});

        for (size_t i = 0; i < implicitArgsVec2.size(); i++) {
            if (isValidOffset(*implicitArgsVec2[i])) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, *implicitArgsVec2[i] + sizeof(uint32_t));
            }
        }

        for (size_t i = 0; i < payloadMappings.explicitArgs.size(); i++) {

            switch (payloadMappings.explicitArgs[i].type) {
            case ArgDescriptor::ArgType::ArgTImage: {
                auto &argImage = payloadMappings.explicitArgs[i].as<ArgDescImage>(false);
                if (isValidOffset(argImage.bindless)) {
                    crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argImage.bindless + sizeof(uint32_t));
                }
            } break;
            case ArgDescriptor::ArgType::ArgTPointer: {
                auto &argPtr = payloadMappings.explicitArgs[i].as<ArgDescPointer>(false);
                if (isValidOffset(argPtr.bindless)) {
                    crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argPtr.bindless + sizeof(uint32_t));
                }
                if (isValidOffset(argPtr.stateless)) {
                    crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argPtr.stateless + argPtr.pointerSize);
                }
            } break;
            case ArgDescriptor::ArgType::ArgTSampler: {
                auto &argSampler = payloadMappings.explicitArgs[i].as<ArgDescSampler>(false);
                UNRECOVERABLE_IF(isValidOffset(argSampler.bindless));
            } break;
            case ArgDescriptor::ArgType::ArgTValue: {
                auto &argVal = payloadMappings.explicitArgs[i].as<ArgDescValue>(false);
                for (size_t i = 0; i < argVal.elements.size(); i++) {
                    UNRECOVERABLE_IF(!isValidOffset(argVal.elements[i].offset));
                    crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argVal.elements[i].offset + argVal.elements[i].size);
                }
            } break;
            default:
                break;
            }
        }

        this->kernelAttributes.crossThreadDataSize = std::max<uint16_t>(this->kernelAttributes.crossThreadDataSize, static_cast<uint16_t>(alignUp(crossThreadDataSize, 32)));
    }

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
                bool reserved : 1;
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
        std::unique_ptr<uint8_t[]> relocatedDebugData;
        const void *igcInfoForGtpin = nullptr;
    } external;

    std::vector<uint8_t> generatedHeaps;
};

} // namespace NEO
