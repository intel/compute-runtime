/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include "igfxfmid.h"
#include "patch_g7.h"
#include "patch_list.h"
#include "patch_shared.h"
#include "program_debug_data.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace NEO {

namespace PatchTokenBinary {

using namespace iOpenCL;

enum class ArgObjectType : uint32_t {
    None = 0,
    Buffer,
    Image,
    Sampler,
    Slm
};

enum class ArgObjectTypeSpecialized : uint32_t {
    None = 0,
    Vme
};

using StackVecUnhandledTokens = StackVec<const SPatchItemHeader *, 4>;
using StackVecByValMap = StackVec<const SPatchDataParameterBuffer *, 8>;
using StackVecStrings = StackVec<const SPatchString *, 4>;

struct KernelArgFromPatchtokens {
    const SPatchKernelArgumentInfo *argInfo = nullptr;
    const SPatchItemHeader *objectArg = nullptr;
    const SPatchDataParameterBuffer *objectId = nullptr;
    ArgObjectType objectType = ArgObjectType::None;
    ArgObjectTypeSpecialized objectTypeSpecialized = ArgObjectTypeSpecialized::None;
    StackVecByValMap byValMap;
    union {
        struct {
            const SPatchDataParameterBuffer *width;
            const SPatchDataParameterBuffer *height;
            const SPatchDataParameterBuffer *depth;
            const SPatchDataParameterBuffer *channelDataType;
            const SPatchDataParameterBuffer *channelOrder;
            const SPatchDataParameterBuffer *arraySize;
            const SPatchDataParameterBuffer *numSamples;
            const SPatchDataParameterBuffer *numMipLevels;
            const SPatchDataParameterBuffer *flatBaseOffset;
            const SPatchDataParameterBuffer *flatWidth;
            const SPatchDataParameterBuffer *flatHeight;
            const SPatchDataParameterBuffer *flatPitch;
        } image;
        struct {
            const SPatchDataParameterBuffer *bufferOffset;
            const SPatchDataParameterBuffer *pureStateful;
        } buffer;
        struct {
            const SPatchDataParameterBuffer *coordinateSnapWaRequired;
            const SPatchDataParameterBuffer *addressMode;
            const SPatchDataParameterBuffer *normalizedCoords;
        } sampler;
        struct {
            const SPatchDataParameterBuffer *token;
        } slm;
        static_assert((sizeof(image) > sizeof(buffer)) && (sizeof(image) > sizeof(sampler)) && (sizeof(image) > sizeof(slm)),
                      "Union initialization based on image wont' initialize whole struct");
    } metadata = {};

    union {
        struct {
            const SPatchDataParameterBuffer *mbBlockType;
            const SPatchDataParameterBuffer *subpixelMode;
            const SPatchDataParameterBuffer *sadAdjustMode;
            const SPatchDataParameterBuffer *searchPathType;
        } vme;
    } metadataSpecialized = {};
};

using StackVecKernelArgs = StackVec<KernelArgFromPatchtokens, 12>;

struct KernelFromPatchtokens {
    DecodeError decodeStatus = DecodeError::Undefined;

    const SKernelBinaryHeaderCommon *header = nullptr;
    ArrayRef<const char> name;
    ArrayRef<const uint8_t> isa;

    struct {
        ArrayRef<const uint8_t> generalState;
        ArrayRef<const uint8_t> dynamicState;
        ArrayRef<const uint8_t> surfaceState;
    } heaps;

    struct {
        ArrayRef<const uint8_t> kernelInfo;
        ArrayRef<const uint8_t> patchList;
    } blobs;

    struct {
        const SPatchSamplerStateArray *samplerStateArray = nullptr;
        const SPatchBindingTableState *bindingTableState = nullptr;
        const SPatchAllocateLocalSurface *allocateLocalSurface = nullptr;
        const SPatchMediaVFEState *mediaVfeState[2] = {nullptr, nullptr};
        const SPatchMediaInterfaceDescriptorLoad *mediaInterfaceDescriptorLoad = nullptr;
        const SPatchThreadPayload *threadPayload = nullptr;
        const SPatchExecutionEnvironment *executionEnvironment = nullptr;
        const SPatchDataParameterStream *dataParameterStream = nullptr;
        const SPatchKernelAttributesInfo *kernelAttributesInfo = nullptr;
        const SPatchAllocateStatelessPrivateSurface *allocateStatelessPrivateSurface = nullptr;
        const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *allocateStatelessConstantMemorySurfaceWithInitialization = nullptr;
        const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *allocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
        const SPatchAllocateStatelessPrintfSurface *allocateStatelessPrintfSurface = nullptr;
        const SPatchAllocateStatelessEventPoolSurface *allocateStatelessEventPoolSurface = nullptr;
        const SPatchAllocateStatelessDefaultDeviceQueueSurface *allocateStatelessDefaultDeviceQueueSurface = nullptr;
        const SPatchAllocateSyncBuffer *allocateSyncBuffer = nullptr;
        const void *allocateRTGlobalBuffer = nullptr;
        const SPatchItemHeader *inlineVmeSamplerInfo = nullptr;
        const SPatchGtpinFreeGRFInfo *gtpinFreeGrfInfo = nullptr;
        const SPatchStateSIP *stateSip = nullptr;
        const SPatchAllocateSystemThreadSurface *allocateSystemThreadSurface = nullptr;
        const SPatchItemHeader *gtpinInfo = nullptr;
        const SPatchFunctionTableInfo *programSymbolTable = nullptr;
        const SPatchFunctionTableInfo *programRelocationTable = nullptr;
        const SPatchFunctionTableInfo *hostAccessTable = nullptr;
        StackVecKernelArgs kernelArgs;
        StackVecStrings strings;
        struct {
            const SPatchDataParameterBuffer *localWorkSize[3] = {};
            const SPatchDataParameterBuffer *localWorkSize2[3] = {};
            const SPatchDataParameterBuffer *enqueuedLocalWorkSize[3] = {};
            const SPatchDataParameterBuffer *numWorkGroups[3] = {};
            const SPatchDataParameterBuffer *globalWorkOffset[3] = {};
            const SPatchDataParameterBuffer *globalWorkSize[3] = {};
            const SPatchDataParameterBuffer *maxWorkGroupSize = nullptr;
            const SPatchDataParameterBuffer *workDimensions = nullptr;
            const SPatchDataParameterBuffer *simdSize = nullptr;
            const SPatchDataParameterBuffer *parentEvent = nullptr;
            const SPatchDataParameterBuffer *privateMemoryStatelessSize = nullptr;
            const SPatchDataParameterBuffer *localMemoryStatelessWindowSize = nullptr;
            const SPatchDataParameterBuffer *localMemoryStatelessWindowStartAddress = nullptr;
            const SPatchDataParameterBuffer *preferredWorkgroupMultiple = nullptr;
            const SPatchDataParameterBuffer *implicitArgsBufferOffset = nullptr;
        } crossThreadPayloadArgs;
    } tokens;

    StackVecUnhandledTokens unhandledTokens;
};

struct ProgramFromPatchtokens {
    DecodeError decodeStatus = DecodeError::Undefined;

    const SProgramBinaryHeader *header = nullptr;
    struct {
        ArrayRef<const uint8_t> programInfo;
        ArrayRef<const uint8_t> patchList;
        ArrayRef<const uint8_t> kernelsInfo;
    } blobs;

    struct {
        StackVec<const SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *, 2> allocateConstantMemorySurface;
        StackVec<const SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *, 2> allocateGlobalMemorySurface;
        StackVec<const SPatchConstantPointerProgramBinaryInfo *, 4> constantPointer;
        StackVec<const SPatchGlobalPointerProgramBinaryInfo *, 4> globalPointer;
        const SPatchFunctionTableInfo *symbolTable = nullptr;
    } programScopeTokens;
    StackVec<KernelFromPatchtokens, 2> kernels;
    StackVec<const SPatchItemHeader *, 4> unhandledTokens;
};

struct KernelArgAttributesFromPatchtokens {
    ArrayRef<const char> addressQualifier;
    ArrayRef<const char> accessQualifier;
    ArrayRef<const char> argName;
    ArrayRef<const char> typeName;
    ArrayRef<const char> typeQualifiers;
};

bool decodeKernelFromPatchtokensBlob(ArrayRef<const uint8_t> kernelBlob, KernelFromPatchtokens &out);
bool decodeProgramFromPatchtokensBlob(ArrayRef<const uint8_t> programBlob, ProgramFromPatchtokens &out);
uint32_t calcKernelChecksum(const ArrayRef<const uint8_t> kernelBlob);
bool hasInvalidChecksum(const KernelFromPatchtokens &decodedKernel);

inline const uint8_t *getInlineData(const SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *ptr) {
    return ptrOffset(reinterpret_cast<const uint8_t *>(ptr), sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo));
}

inline const uint8_t *getInlineData(const SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *ptr) {
    return ptrOffset(reinterpret_cast<const uint8_t *>(ptr), sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));
}

inline const uint8_t *getInlineData(const SPatchString *ptr) {
    return ptrOffset(reinterpret_cast<const uint8_t *>(ptr), sizeof(SPatchString));
}

inline uint64_t getPerHwThreadPrivateSurfaceSize(const SPatchAllocateStatelessPrivateSurface &ptr, uint32_t simdSize) {
    uint32_t multiplier = ptr.IsSimtThread ? simdSize : 1U;
    return static_cast<uint64_t>(ptr.PerThreadPrivateMemorySize) * multiplier;
}

const KernelArgAttributesFromPatchtokens getInlineData(const SPatchKernelArgumentInfo *ptr);

const iOpenCL::SProgramBinaryHeader *decodeProgramHeader(const ArrayRef<const uint8_t> programBlob);

inline bool isValidPatchTokenBinary(const ArrayRef<const uint8_t> programBlob) {
    return nullptr != decodeProgramHeader(programBlob);
}

} // namespace PatchTokenBinary

} // namespace NEO
