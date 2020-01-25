/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "patchtokens_decoder.h"

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/hash.h"
#include "core/helpers/ptr_math.h"

#include <algorithm>

namespace NEO {

namespace PatchTokenBinary {

struct PatchTokensStreamReader {
    const ArrayRef<const uint8_t> data;
    PatchTokensStreamReader(ArrayRef<const uint8_t> data) : data(data) {}

    template <typename DecodePosT>
    bool notEnoughDataLeft(DecodePosT *decodePos, size_t requestedSize) {
        return getDataSizeLeft(decodePos) < requestedSize;
    }

    template <typename T, typename DecodePosT>
    constexpr bool notEnoughDataLeft(DecodePosT *decodePos) {
        return notEnoughDataLeft(decodePos, sizeof(T));
    }

    template <typename... ArgsT>
    bool enoughDataLeft(ArgsT &&... args) {
        return false == notEnoughDataLeft(std::forward<ArgsT>(args)...);
    }

    template <typename T, typename... ArgsT>
    bool enoughDataLeft(ArgsT &&... args) {
        return false == notEnoughDataLeft<T>(std::forward<ArgsT>(args)...);
    }

    template <typename DecodePosT>
    size_t getDataSizeLeft(DecodePosT *decodePos) {
        auto dataConsumed = ptrDiff(decodePos, data.begin());
        UNRECOVERABLE_IF(dataConsumed > data.size());
        return data.size() - dataConsumed;
    }
};

template <typename T>
inline void assignToken(const T *&dst, const SPatchItemHeader *src) {
    dst = reinterpret_cast<const T *>(src);
}

inline KernelArgFromPatchtokens &getKernelArg(KernelFromPatchtokens &kernel, size_t argNum, ArgObjectType type = ArgObjectType::None, ArgObjectTypeSpecialized typeSpecialized = ArgObjectTypeSpecialized::None) {
    if (kernel.tokens.kernelArgs.size() < argNum + 1) {
        kernel.tokens.kernelArgs.resize(argNum + 1);
    }
    auto &arg = kernel.tokens.kernelArgs[argNum];
    if (arg.objectType == ArgObjectType::None) {
        arg.objectType = type;
    } else if ((arg.objectType != type) && (type != ArgObjectType::None)) {
        kernel.decodeStatus = DecoderError::InvalidBinary;
        DBG_LOG(LogPatchTokens, "\n Mismatched metadata for kernel arg :", argNum);
        DEBUG_BREAK_IF(true);
    }

    if (arg.objectTypeSpecialized == ArgObjectTypeSpecialized::None) {
        arg.objectTypeSpecialized = typeSpecialized;
    } else if (typeSpecialized != ArgObjectTypeSpecialized::None) {
        UNRECOVERABLE_IF(arg.objectTypeSpecialized != typeSpecialized);
    }

    return arg;
}

inline void assignArgInfo(KernelFromPatchtokens &kernel, const SPatchItemHeader *src) {
    auto argInfoToken = reinterpret_cast<const SPatchKernelArgumentInfo *>(src);
    getKernelArg(kernel, argInfoToken->ArgumentNumber, ArgObjectType::None).argInfo = argInfoToken;
}

template <typename T>
inline uint32_t getArgNum(const SPatchItemHeader *argToken) {
    return reinterpret_cast<const T *>(argToken)->ArgumentNumber;
}

inline void assignArg(KernelFromPatchtokens &kernel, const SPatchItemHeader *src) {
    uint32_t argNum = 0;
    ArgObjectType type = ArgObjectType::Buffer;
    switch (src->Token) {
    default:
        UNRECOVERABLE_IF(src->Token != PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT);
        argNum = getArgNum<SPatchSamplerKernelArgument>(src);
        type = ArgObjectType::Sampler;
        break;
    case PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT:
        argNum = getArgNum<SPatchImageMemoryObjectKernelArgument>(src);
        type = ArgObjectType::Image;
        break;
    case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        argNum = getArgNum<SPatchGlobalMemoryObjectKernelArgument>(src);
        break;
    case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        argNum = getArgNum<SPatchStatelessGlobalMemoryObjectKernelArgument>(src);
        break;
    case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT:
        argNum = getArgNum<SPatchStatelessConstantMemoryObjectKernelArgument>(src);
        break;
    case PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT:
        argNum = getArgNum<SPatchStatelessDeviceQueueKernelArgument>(src);
        break;
    }

    getKernelArg(kernel, argNum, type).objectArg = src;
}

inline void assignToken(StackVecStrings &stringVec, const SPatchItemHeader *src) {
    auto stringToken = reinterpret_cast<const SPatchString *>(src);
    if (stringVec.size() < stringToken->Index + 1) {
        stringVec.resize(stringToken->Index + 1);
    }
    stringVec[stringToken->Index] = stringToken;
}

template <size_t S>
inline void assignTokenInArray(const SPatchDataParameterBuffer *(&tokensArray)[S], const SPatchDataParameterBuffer *src, StackVecUnhandledTokens &unhandledTokens) {
    auto sourceIndex = src->SourceOffset >> 2;
    if (sourceIndex >= S) {
        DBG_LOG(LogPatchTokens, "\n  .Type", "Unhandled sourceIndex ", sourceIndex);
        DEBUG_BREAK_IF(true);
        unhandledTokens.push_back(src);
        return;
    }
    assignToken(tokensArray[sourceIndex], src);
}

template <typename PatchT, size_t NumInlineEl>
inline void addTok(StackVec<const PatchT *, NumInlineEl> &tokensVec, const SPatchItemHeader *src) {
    tokensVec.push_back(reinterpret_cast<const PatchT *>(src));
}

inline void decodeKernelDataParameterToken(const SPatchDataParameterBuffer *token, KernelFromPatchtokens &out) {
    auto &crossthread = out.tokens.crossThreadPayloadArgs;
    auto sourceIndex = token->SourceOffset >> 2;
    auto argNum = token->ArgumentNumber;

    switch (token->Type) {
    default:
        DBG_LOG(LogPatchTokens, "\n  .Type", "Unhandled SPatchDataParameterBuffer ", token->Type);
        DEBUG_BREAK_IF(true);
        out.unhandledTokens.push_back(token);
        break;

    case DATA_PARAMETER_KERNEL_ARGUMENT:
        getKernelArg(out, argNum, ArgObjectType::None).byValMap.push_back(token);
        break;

    case DATA_PARAMETER_LOCAL_WORK_SIZE: {
        if (sourceIndex >= 3) {
            DBG_LOG(LogPatchTokens, "\n  .Type", "Unhandled sourceIndex ", sourceIndex);
            DEBUG_BREAK_IF(true);
            out.unhandledTokens.push_back(token);
            return;
        }
        auto localWorkSizeArray = (crossthread.localWorkSize[sourceIndex] == nullptr)
                                      ? crossthread.localWorkSize
                                      : crossthread.localWorkSize2;
        localWorkSizeArray[sourceIndex] = token;
        break;
    }

    case DATA_PARAMETER_GLOBAL_WORK_OFFSET:
        assignTokenInArray(crossthread.globalWorkOffset, token, out.unhandledTokens);
        break;
    case DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE:
        assignTokenInArray(crossthread.enqueuedLocalWorkSize, token, out.unhandledTokens);
        break;
    case DATA_PARAMETER_GLOBAL_WORK_SIZE:
        assignTokenInArray(crossthread.globalWorkSize, token, out.unhandledTokens);
        break;
    case DATA_PARAMETER_NUM_WORK_GROUPS:
        assignTokenInArray(crossthread.numWorkGroups, token, out.unhandledTokens);
        break;
    case DATA_PARAMETER_MAX_WORKGROUP_SIZE:
        crossthread.maxWorkGroupSize = token;
        break;
    case DATA_PARAMETER_WORK_DIMENSIONS:
        crossthread.workDimensions = token;
        break;
    case DATA_PARAMETER_SIMD_SIZE:
        crossthread.simdSize = token;
        break;

    case DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE:
        crossthread.privateMemoryStatelessSize = token;
        break;
    case DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE:
        crossthread.localMemoryStatelessWindowSize = token;
        break;
    case DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS:
        crossthread.localMemoryStatelessWindowStartAddress = token;
        break;

    case DATA_PARAMETER_OBJECT_ID:
        getKernelArg(out, argNum, ArgObjectType::None).objectId = token;
        break;

    case DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES: {
        auto &kernelArg = getKernelArg(out, argNum, ArgObjectType::Slm);
        kernelArg.byValMap.push_back(token);
        kernelArg.metadata.slm.token = token;
    } break;

    case DATA_PARAMETER_BUFFER_OFFSET:
        getKernelArg(out, argNum, ArgObjectType::Buffer).metadata.buffer.bufferOffset = token;
        break;
    case DATA_PARAMETER_BUFFER_STATEFUL:
        getKernelArg(out, argNum, ArgObjectType::Buffer).metadata.buffer.pureStateful = token;
        break;

    case DATA_PARAMETER_IMAGE_WIDTH:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.width = token;
        break;
    case DATA_PARAMETER_IMAGE_HEIGHT:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.height = token;
        break;
    case DATA_PARAMETER_IMAGE_DEPTH:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.depth = token;
        break;
    case DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.channelDataType = token;
        break;
    case DATA_PARAMETER_IMAGE_CHANNEL_ORDER:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.channelOrder = token;
        break;
    case DATA_PARAMETER_IMAGE_ARRAY_SIZE:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.arraySize = token;
        break;
    case DATA_PARAMETER_IMAGE_NUM_SAMPLES:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.numSamples = token;
        break;
    case DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.numMipLevels = token;
        break;
    case DATA_PARAMETER_FLAT_IMAGE_BASEOFFSET:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.flatBaseOffset = token;
        break;
    case DATA_PARAMETER_FLAT_IMAGE_WIDTH:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.flatWidth = token;
        break;
    case DATA_PARAMETER_FLAT_IMAGE_HEIGHT:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.flatHeight = token;
        break;
    case DATA_PARAMETER_FLAT_IMAGE_PITCH:
        getKernelArg(out, argNum, ArgObjectType::Image).metadata.image.flatPitch = token;
        break;

    case DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED:
        getKernelArg(out, argNum, ArgObjectType::Sampler).metadata.sampler.coordinateSnapWaRequired = token;
        break;
    case DATA_PARAMETER_SAMPLER_ADDRESS_MODE:
        getKernelArg(out, argNum, ArgObjectType::Sampler).metadata.sampler.addressMode = token;
        break;
    case DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS:
        getKernelArg(out, argNum, ArgObjectType::Sampler).metadata.sampler.normalizedCoords = token;
        break;

    case DATA_PARAMETER_VME_MB_BLOCK_TYPE:
        getKernelArg(out, argNum, ArgObjectType::None, ArgObjectTypeSpecialized::Vme).metadataSpecialized.vme.mbBlockType = token;
        break;
    case DATA_PARAMETER_VME_SUBPIXEL_MODE:
        getKernelArg(out, argNum, ArgObjectType::None, ArgObjectTypeSpecialized::Vme).metadataSpecialized.vme.subpixelMode = token;
        break;
    case DATA_PARAMETER_VME_SAD_ADJUST_MODE:
        getKernelArg(out, argNum, ArgObjectType::None, ArgObjectTypeSpecialized::Vme).metadataSpecialized.vme.sadAdjustMode = token;
        break;
    case DATA_PARAMETER_VME_SEARCH_PATH_TYPE:
        getKernelArg(out, argNum, ArgObjectType::None, ArgObjectTypeSpecialized::Vme).metadataSpecialized.vme.searchPathType = token;
        break;

    case DATA_PARAMETER_PARENT_EVENT:
        crossthread.parentEvent = token;
        break;
    case DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE:
        crossthread.childBlockSimdSize.push_back(token);
        break;
    case DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE:
        crossthread.preferredWorkgroupMultiple = token;
        break;

    case DATA_PARAMETER_NUM_HARDWARE_THREADS:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_PRINTF_SURFACE_SIZE:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_IMAGE_SRGB_CHANNEL_ORDER:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_STAGE_IN_GRID_ORIGIN:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_STAGE_IN_GRID_SIZE:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_LOCAL_ID:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_EXECUTION_MASK:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_VME_IMAGE_TYPE:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case DATA_PARAMETER_VME_MB_SKIP_BLOCK_TYPE:
        // ignored intentionally
        break;
    }
}

inline bool decodeToken(const SPatchItemHeader *token, KernelFromPatchtokens &out) {
    switch (token->Token) {
    default: {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "Unknown kernel-scope Patch Token: %d\n", token->Token);
        DEBUG_BREAK_IF(true);
        out.unhandledTokens.push_back(token);
        break;
    }
    case PATCH_TOKEN_SAMPLER_STATE_ARRAY:
        assignToken(out.tokens.samplerStateArray, token);
        break;
    case PATCH_TOKEN_BINDING_TABLE_STATE:
        assignToken(out.tokens.bindingTableState, token);
        break;
    case PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE:
        assignToken(out.tokens.allocateLocalSurface, token);
        break;
    case PATCH_TOKEN_MEDIA_VFE_STATE:
        assignToken(out.tokens.mediaVfeState[0], token);
        break;
    case PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1:
        assignToken(out.tokens.mediaVfeState[1], token);
        break;
    case PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD:
        assignToken(out.tokens.mediaInterfaceDescriptorLoad, token);
        break;
    case PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA:
        assignToken(out.tokens.interfaceDescriptorData, token);
        break;
    case PATCH_TOKEN_THREAD_PAYLOAD:
        assignToken(out.tokens.threadPayload, token);
        break;
    case PATCH_TOKEN_EXECUTION_ENVIRONMENT:
        assignToken(out.tokens.executionEnvironment, token);
        break;

    case PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO:
        assignToken(out.tokens.kernelAttributesInfo, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY:
        assignToken(out.tokens.allocateStatelessPrivateSurface, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION:
        assignToken(out.tokens.allocateStatelessConstantMemorySurfaceWithInitialization, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION:
        assignToken(out.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE:
        assignToken(out.tokens.allocateStatelessPrintfSurface, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE:
        assignToken(out.tokens.allocateStatelessEventPoolSurface, token);
        break;
    case PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE:
        assignToken(out.tokens.allocateStatelessDefaultDeviceQueueSurface, token);
        break;
    case PATCH_TOKEN_STRING:
        assignToken(out.tokens.strings, token);
        break;
    case PATCH_TOKEN_INLINE_VME_SAMPLER_INFO:
        assignToken(out.tokens.inlineVmeSamplerInfo, token);
        break;
    case PATCH_TOKEN_GTPIN_FREE_GRF_INFO:
        assignToken(out.tokens.gtpinFreeGrfInfo, token);
        break;
    case PATCH_TOKEN_GTPIN_INFO:
        assignToken(out.tokens.gtpinInfo, token);
        break;
    case PATCH_TOKEN_STATE_SIP:
        assignToken(out.tokens.stateSip, token);
        break;
    case PATCH_TOKEN_ALLOCATE_SIP_SURFACE:
        assignToken(out.tokens.allocateSystemThreadSurface, token);
        break;
    case PATCH_TOKEN_PROGRAM_SYMBOL_TABLE:
        assignToken(out.tokens.programSymbolTable, token);
        break;
    case PATCH_TOKEN_PROGRAM_RELOCATION_TABLE:
        assignToken(out.tokens.programRelocationTable, token);
        break;
    case PATCH_TOKEN_KERNEL_ARGUMENT_INFO:
        assignArgInfo(out, token);
        break;

    case PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT:
        assignArg(out, token);
        break;

    case PATCH_TOKEN_DATA_PARAMETER_STREAM:
        assignToken(out.tokens.dataParameterStream, token);
        break;
    case PATCH_TOKEN_DATA_PARAMETER_BUFFER: {
        auto tokDataP = reinterpret_cast<const SPatchDataParameterBuffer *>(token);
        decodeKernelDataParameterToken(tokDataP, out);
    } break;

    case PATCH_TOKEN_ALLOCATE_SYNC_BUFFER: {
        assignToken(out.tokens.allocateSyncBuffer, token);
    } break;
    }

    return out.decodeStatus != DecoderError::InvalidBinary;
}

inline bool decodeToken(const SPatchItemHeader *token, ProgramFromPatchtokens &out) {
    auto &progTok = out.programScopeTokens;
    switch (token->Token) {
    default: {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "Unknown program-scope Patch Token: %d\n", token->Token);
        DEBUG_BREAK_IF(true);
        out.unhandledTokens.push_back(token);
        break;
    }
    case PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:
        addTok(progTok.allocateConstantMemorySurface, token);
        break;
    case PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO:
        addTok(progTok.allocateGlobalMemorySurface, token);
        break;
    case PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO:
        addTok(progTok.globalPointer, token);
        break;
    case PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO:
        addTok(progTok.constantPointer, token);
        break;
    case PATCH_TOKEN_PROGRAM_SYMBOL_TABLE:
        assignToken(progTok.symbolTable, token);
        break;
    }
    return true;
}

template <typename DecodeContext>
inline size_t getPatchTokenTotalSize(PatchTokensStreamReader stream, const SPatchItemHeader *token);

template <>
inline size_t getPatchTokenTotalSize<KernelFromPatchtokens>(PatchTokensStreamReader stream, const SPatchItemHeader *token) {
    return token->Size;
}

template <>
inline size_t getPatchTokenTotalSize<ProgramFromPatchtokens>(PatchTokensStreamReader stream, const SPatchItemHeader *token) {
    size_t tokSize = token->Size;
    switch (token->Token) {
    default:
        return tokSize;
    case PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO:
        return stream.enoughDataLeft<SPatchAllocateConstantMemorySurfaceProgramBinaryInfo>(token)
                   ? tokSize + reinterpret_cast<const SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *>(token)->InlineDataSize
                   : std::numeric_limits<size_t>::max();
    case PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO:
        return stream.enoughDataLeft<SPatchAllocateConstantMemorySurfaceProgramBinaryInfo>(token)
                   ? tokSize + reinterpret_cast<const SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *>(token)->InlineDataSize
                   : std::numeric_limits<size_t>::max();
    }
}

template <typename OutT>
inline bool decodePatchList(PatchTokensStreamReader patchListStream, OutT &out) {
    auto decodePos = patchListStream.data.begin();
    auto decodeEnd = patchListStream.data.end();

    bool decodeSuccess = true;
    while ((decodePos + sizeof(SPatchItemHeader) <= decodeEnd) && decodeSuccess) {
        auto token = reinterpret_cast<const SPatchItemHeader *>(decodePos);
        size_t tokenTotalSize = getPatchTokenTotalSize<OutT>(patchListStream, token);
        decodeSuccess = patchListStream.enoughDataLeft(decodePos, tokenTotalSize);
        decodeSuccess = decodeSuccess && (tokenTotalSize > 0U);
        decodeSuccess = decodeSuccess && decodeToken(token, out);
        decodePos = ptrOffset(decodePos, tokenTotalSize);
    }

    return decodeSuccess;
}

bool decodeKernelFromPatchtokensBlob(ArrayRef<const uint8_t> data, KernelFromPatchtokens &out) {
    PatchTokensStreamReader stream{data};
    auto decodePos = stream.data.begin();
    out.decodeStatus = DecoderError::Undefined;
    if (stream.notEnoughDataLeft<SKernelBinaryHeaderCommon>(decodePos)) {
        out.decodeStatus = DecoderError::InvalidBinary;
        return false;
    }

    out.header = reinterpret_cast<const SKernelBinaryHeaderCommon *>(decodePos);

    auto kernelInfoBlobSize = sizeof(SKernelBinaryHeaderCommon) + out.header->KernelNameSize + out.header->KernelHeapSize + out.header->GeneralStateHeapSize + out.header->DynamicStateHeapSize + out.header->SurfaceStateHeapSize + out.header->PatchListSize;

    if (stream.notEnoughDataLeft(decodePos, kernelInfoBlobSize)) {
        out.decodeStatus = DecoderError::InvalidBinary;
        return false;
    }

    out.blobs.kernelInfo = ArrayRef<const uint8_t>(stream.data.begin(), kernelInfoBlobSize);
    decodePos = ptrOffset(decodePos, sizeof(SKernelBinaryHeaderCommon));

    auto kernelName = reinterpret_cast<const char *>(decodePos);
    out.name = ArrayRef<const char>(kernelName, out.header->KernelNameSize);
    decodePos = ptrOffset(decodePos, out.name.size());

    out.isa = ArrayRef<const uint8_t>(decodePos, out.header->KernelHeapSize);
    decodePos = ptrOffset(decodePos, out.isa.size());

    out.heaps.generalState = ArrayRef<const uint8_t>(decodePos, out.header->GeneralStateHeapSize);
    decodePos = ptrOffset(decodePos, out.heaps.generalState.size());

    out.heaps.dynamicState = ArrayRef<const uint8_t>(decodePos, out.header->DynamicStateHeapSize);
    decodePos = ptrOffset(decodePos, out.heaps.dynamicState.size());

    out.heaps.surfaceState = ArrayRef<const uint8_t>(decodePos, out.header->SurfaceStateHeapSize);
    decodePos = ptrOffset(decodePos, out.heaps.surfaceState.size());

    out.blobs.patchList = ArrayRef<const uint8_t>(decodePos, out.header->PatchListSize);

    if (false == decodePatchList(out.blobs.patchList, out)) {
        out.decodeStatus = DecoderError::InvalidBinary;
        return false;
    }

    out.decodeStatus = DecoderError::Success;
    return true;
}

inline bool decodeProgramHeader(ProgramFromPatchtokens &decodedProgram) {
    auto decodePos = decodedProgram.blobs.programInfo.begin();
    PatchTokensStreamReader stream{decodedProgram.blobs.programInfo};
    if (stream.notEnoughDataLeft<SProgramBinaryHeader>(decodePos)) {
        return false;
    }

    decodedProgram.header = reinterpret_cast<const SProgramBinaryHeader *>(decodePos);
    if (decodedProgram.header->Magic != MAGIC_CL) {
        return false;
    }
    decodePos = ptrOffset(decodePos, sizeof(SProgramBinaryHeader));

    if (stream.notEnoughDataLeft(decodePos, decodedProgram.header->PatchListSize)) {
        return false;
    }
    decodedProgram.blobs.patchList = ArrayRef<const uint8_t>(decodePos, decodedProgram.header->PatchListSize);
    decodePos = ptrOffset(decodePos, decodedProgram.blobs.patchList.size());

    decodedProgram.blobs.kernelsInfo = ArrayRef<const uint8_t>(decodePos, stream.getDataSizeLeft(decodePos));
    return true;
}

inline bool decodeKernels(ProgramFromPatchtokens &decodedProgram) {
    auto numKernels = decodedProgram.header->NumberOfKernels;
    decodedProgram.kernels.reserve(decodedProgram.header->NumberOfKernels);
    const uint8_t *decodePos = decodedProgram.blobs.kernelsInfo.begin();
    bool decodeSuccess = true;
    PatchTokensStreamReader stream{decodedProgram.blobs.kernelsInfo};
    for (uint32_t i = 0; (i < numKernels) && decodeSuccess; i++) {
        decodedProgram.kernels.resize(decodedProgram.kernels.size() + 1);
        auto &currKernelInfo = *decodedProgram.kernels.rbegin();
        auto kernelDataLeft = ArrayRef<const uint8_t>(decodePos, stream.getDataSizeLeft(decodePos));
        decodeSuccess = decodeKernelFromPatchtokensBlob(kernelDataLeft, currKernelInfo);
        decodePos = ptrOffset(decodePos, currKernelInfo.blobs.kernelInfo.size());
    }
    return decodeSuccess;
}

bool decodeProgramFromPatchtokensBlob(ArrayRef<const uint8_t> blob, ProgramFromPatchtokens &out) {
    out.blobs.programInfo = blob;
    bool decodeSuccess = decodeProgramHeader(out);
    decodeSuccess = decodeSuccess && decodeKernels(out);
    decodeSuccess = decodeSuccess && decodePatchList(out.blobs.patchList, out);
    out.decodeStatus = decodeSuccess ? DecoderError::Success : DecoderError::InvalidBinary;

    return decodeSuccess;
}

uint32_t calcKernelChecksum(const ArrayRef<const uint8_t> kernelBlob) {
    UNRECOVERABLE_IF(kernelBlob.size() <= sizeof(SKernelBinaryHeaderCommon));
    auto dataToHash = ArrayRef<const uint8_t>(ptrOffset(kernelBlob.begin(), sizeof(SKernelBinaryHeaderCommon)), kernelBlob.end());
    uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(dataToHash.begin()), dataToHash.size());
    uint32_t checksum = hashValue & 0xFFFFFFFF;
    return checksum;
}

bool hasInvalidChecksum(const KernelFromPatchtokens &decodedKernel) {
    uint32_t decodedChecksum = decodedKernel.header->CheckSum;
    uint32_t calculatedChecksum = NEO::PatchTokenBinary::calcKernelChecksum(decodedKernel.blobs.kernelInfo);
    return decodedChecksum != calculatedChecksum;
}

const KernelArgAttributesFromPatchtokens getInlineData(const SPatchKernelArgumentInfo *ptr) {
    KernelArgAttributesFromPatchtokens ret = {};
    UNRECOVERABLE_IF(ptr == nullptr);
    auto decodePos = reinterpret_cast<const char *>(ptr + 1);
    auto bounds = reinterpret_cast<const char *>(ptr) + ptr->Size;
    ret.addressQualifier = ArrayRef<const char>(decodePos, std::min(decodePos + ptr->AddressQualifierSize, bounds));
    decodePos += ret.addressQualifier.size();

    ret.accessQualifier = ArrayRef<const char>(decodePos, std::min(decodePos + ptr->AccessQualifierSize, bounds));
    decodePos += ret.accessQualifier.size();

    ret.argName = ArrayRef<const char>(decodePos, std::min(decodePos + ptr->ArgumentNameSize, bounds));
    decodePos += ret.argName.size();

    ret.typeName = ArrayRef<const char>(decodePos, std::min(decodePos + ptr->TypeNameSize, bounds));
    decodePos += ret.typeName.size();

    ret.typeQualifiers = ArrayRef<const char>(decodePos, std::min(decodePos + ptr->TypeQualifierSize, bounds));
    return ret;
}

} // namespace PatchTokenBinary

} // namespace NEO
