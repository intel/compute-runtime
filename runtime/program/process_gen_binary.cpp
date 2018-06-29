/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/memory_manager.h"
#include "patch_list.h"
#include "patch_shared.h"
#include "program_debug_data.h"
#include "program.h"
#include "runtime/kernel/kernel.h"

#include <algorithm>

using namespace iOpenCL;

namespace OCLRT {
extern bool familyEnabled[];

const KernelInfo *Program::getKernelInfo(
    const char *kernelName) const {
    if (kernelName == nullptr) {
        return nullptr;
    }

    auto it = std::find_if(kernelInfoArray.begin(), kernelInfoArray.end(),
                           [=](const KernelInfo *kInfo) { return (0 == strcmp(kInfo->name.c_str(), kernelName)); });

    return (it != kernelInfoArray.end()) ? *it : nullptr;
}

size_t Program::getNumKernels() const {
    return kernelInfoArray.size();
}

const KernelInfo *Program::getKernelInfo(size_t ordinal) const {
    DEBUG_BREAK_IF(ordinal >= kernelInfoArray.size());
    return kernelInfoArray[ordinal];
}

std::string Program::getKernelNamesString() const {
    std::string semiColonDelimitedKernelNameStr;

    for (uint32_t i = 0; i < kernelInfoArray.size(); i++) {
        semiColonDelimitedKernelNameStr += kernelInfoArray[i]->name;
        if ((i + 1) != kernelInfoArray.size()) {
            semiColonDelimitedKernelNameStr += ";";
        }
    }

    return semiColonDelimitedKernelNameStr;
}

size_t Program::processKernel(
    const void *pKernelBlob,
    cl_int &retVal) {
    size_t sizeProcessed = 0;

    do {
        auto pKernelInfo = KernelInfo::create();
        if (!pKernelInfo) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        auto pCurKernelPtr = pKernelBlob;
        pKernelInfo->heapInfo.pBlob = pKernelBlob;

        pKernelInfo->heapInfo.pKernelHeader = reinterpret_cast<const SKernelBinaryHeaderCommon *>(pCurKernelPtr);
        pCurKernelPtr = ptrOffset(pCurKernelPtr, sizeof(SKernelBinaryHeaderCommon));

        std::string readName{reinterpret_cast<const char *>(pCurKernelPtr), pKernelInfo->heapInfo.pKernelHeader->KernelNameSize};
        pKernelInfo->name = readName.c_str();
        pCurKernelPtr = ptrOffset(pCurKernelPtr, pKernelInfo->heapInfo.pKernelHeader->KernelNameSize);

        pKernelInfo->heapInfo.pKernelHeap = pCurKernelPtr;
        pCurKernelPtr = ptrOffset(pCurKernelPtr, pKernelInfo->heapInfo.pKernelHeader->KernelHeapSize);

        pKernelInfo->heapInfo.pGsh = pCurKernelPtr;
        pCurKernelPtr = ptrOffset(pCurKernelPtr, pKernelInfo->heapInfo.pKernelHeader->GeneralStateHeapSize);

        pKernelInfo->heapInfo.pDsh = pCurKernelPtr;
        pCurKernelPtr = ptrOffset(pCurKernelPtr, pKernelInfo->heapInfo.pKernelHeader->DynamicStateHeapSize);

        pKernelInfo->heapInfo.pSsh = const_cast<void *>(pCurKernelPtr);
        pCurKernelPtr = ptrOffset(pCurKernelPtr, pKernelInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize);

        pKernelInfo->heapInfo.pPatchList = pCurKernelPtr;

        retVal = parsePatchList(*pKernelInfo);
        if (retVal != CL_SUCCESS) {
            delete pKernelInfo;

            sizeProcessed = ptrDiff(pCurKernelPtr, pKernelBlob);
            break;
        }

        auto pKernelHeader = pKernelInfo->heapInfo.pKernelHeader;
        auto pKernel = ptrOffset(pKernelBlob, sizeof(SKernelBinaryHeaderCommon));

        if (genBinary)
            pKernelInfo->gpuPointerSize = reinterpret_cast<const SProgramBinaryHeader *>(genBinary)->GPUPointerSizeInBytes;

        uint32_t kernelSize =
            pKernelHeader->DynamicStateHeapSize +
            pKernelHeader->GeneralStateHeapSize +
            pKernelHeader->KernelHeapSize +
            pKernelHeader->KernelNameSize +
            pKernelHeader->PatchListSize +
            pKernelHeader->SurfaceStateHeapSize;

        pKernelInfo->heapInfo.blobSize = kernelSize + sizeof(SKernelBinaryHeaderCommon);

        uint32_t kernelCheckSum = pKernelInfo->heapInfo.pKernelHeader->CheckSum;

        uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(pKernel), kernelSize);

        uint32_t calcCheckSum = hashValue & 0xFFFFFFFF;
        pKernelInfo->isValid = (calcCheckSum == kernelCheckSum);

        retVal = CL_SUCCESS;
        sizeProcessed = sizeof(SKernelBinaryHeaderCommon) + kernelSize;
        kernelInfoArray.push_back(pKernelInfo);
        if (pKernelInfo->hasDeviceEnqueue()) {
            parentKernelInfoArray.push_back(pKernelInfo);
        }
        if (pKernelInfo->requiresSubgroupIndependentForwardProgress()) {
            subgroupKernelInfoArray.push_back(pKernelInfo);
        }
    } while (false);

    return sizeProcessed;
}

cl_int Program::parsePatchList(KernelInfo &kernelInfo) {
    cl_int retVal = CL_SUCCESS;

    auto pPatchList = kernelInfo.heapInfo.pPatchList;
    auto patchListSize = kernelInfo.heapInfo.pKernelHeader->PatchListSize;
    auto pCurPatchListPtr = pPatchList;
    uint32_t PrivateMemoryStatelessSizeOffset = 0xFFffFFff;
    uint32_t LocalMemoryStatelessWindowSizeOffset = 0xFFffFFff;
    uint32_t LocalMemoryStatelessWindowStartAddressOffset = 0xFFffFFff;

    //Speed up containers by giving some pre-allocated storage
    kernelInfo.kernelArgInfo.reserve(10);
    kernelInfo.patchInfo.kernelArgumentInfo.reserve(10);
    kernelInfo.patchInfo.dataParameterBuffers.reserve(20);

    DBG_LOG(LogPatchTokens, "\nPATCH_TOKENs for kernel", kernelInfo.name);

    while (ptrDiff(pCurPatchListPtr, pPatchList) < patchListSize) {
        uint32_t index = 0;
        uint32_t argNum = 0;
        auto pPatch = reinterpret_cast<const SPatchItemHeader *>(pCurPatchListPtr);
        const SPatchDataParameterBuffer *pDataParameterBuffer = nullptr;

        switch (pPatch->Token) {
        case PATCH_TOKEN_SAMPLER_STATE_ARRAY:
            kernelInfo.patchInfo.samplerStateArray =
                reinterpret_cast<const SPatchSamplerStateArray *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.SAMPLER_STATE_ARRAY", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .Offset", kernelInfo.patchInfo.samplerStateArray->Offset,
                    "\n  .Count", kernelInfo.patchInfo.samplerStateArray->Count,
                    "\n  .BorderColorOffset", kernelInfo.patchInfo.samplerStateArray->BorderColorOffset);
            break;

        case PATCH_TOKEN_BINDING_TABLE_STATE:
            kernelInfo.patchInfo.bindingTableState =
                reinterpret_cast<const SPatchBindingTableState *>(pPatch);
            kernelInfo.usesSsh = (kernelInfo.patchInfo.bindingTableState->Count > 0);
            DBG_LOG(LogPatchTokens,
                    "\n.BINDING_TABLE_STATE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .Offset", kernelInfo.patchInfo.bindingTableState->Offset,
                    "\n  .Count", kernelInfo.patchInfo.bindingTableState->Count,
                    "\n  .SurfaceStateOffset", kernelInfo.patchInfo.bindingTableState->SurfaceStateOffset);
            break;

        case PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE:
            kernelInfo.patchInfo.localsurface =
                reinterpret_cast<const SPatchAllocateLocalSurface *>(pPatch);
            kernelInfo.workloadInfo.slmStaticSize = kernelInfo.patchInfo.localsurface->TotalInlineLocalMemorySize;
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_LOCAL_SURFACE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .TotalInlineLocalMemorySize", kernelInfo.patchInfo.localsurface->TotalInlineLocalMemorySize);
            break;

        case PATCH_TOKEN_MEDIA_VFE_STATE:
            kernelInfo.patchInfo.mediavfestate =
                reinterpret_cast<const SPatchMediaVFEState *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.MEDIA_VFE_STATE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ScratchSpaceOffset", kernelInfo.patchInfo.mediavfestate->ScratchSpaceOffset,
                    "\n  .PerThreadScratchSpace", kernelInfo.patchInfo.mediavfestate->PerThreadScratchSpace);
            break;

        case PATCH_TOKEN_DATA_PARAMETER_BUFFER:
            DBG_LOG(LogPatchTokens,
                    "\n.DATA_PARAMETER_BUFFER", pPatch->Token,
                    "\n  .Size", pPatch->Size);

            pDataParameterBuffer = reinterpret_cast<const SPatchDataParameterBuffer *>(pPatch);
            kernelInfo.patchInfo.dataParameterBuffers.push_back(
                pDataParameterBuffer);
            argNum = pDataParameterBuffer->ArgumentNumber;
            switch (pDataParameterBuffer->Type) {
            case DATA_PARAMETER_KERNEL_ARGUMENT:
                kernelInfo.storeKernelArgument(pDataParameterBuffer);
                DBG_LOG(LogPatchTokens, "\n  .Type", "KERNEL_ARGUMENT");
                break;

            case DATA_PARAMETER_LOCAL_WORK_SIZE: {
                DBG_LOG(LogPatchTokens, "\n  .Type", "LOCAL_WORK_SIZE");
                index = pDataParameterBuffer->SourceOffset / sizeof(uint32_t);
                if (kernelInfo.workloadInfo.localWorkSizeOffsets[2] == WorkloadInfo::undefinedOffset) {
                    kernelInfo.workloadInfo.localWorkSizeOffsets[index] =
                        pDataParameterBuffer->Offset;
                } else {
                    kernelInfo.workloadInfo.localWorkSizeOffsets2[index] =
                        pDataParameterBuffer->Offset;
                }
                break;
            }

            case DATA_PARAMETER_GLOBAL_WORK_OFFSET:
                DBG_LOG(LogPatchTokens, "\n  .Type", "GLOBAL_WORK_OFFSET");
                index = pDataParameterBuffer->SourceOffset / sizeof(uint32_t);
                kernelInfo.workloadInfo.globalWorkOffsetOffsets[index] =
                    pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "ENQUEUED_LOCAL_WORK_SIZE");
                index = pDataParameterBuffer->SourceOffset / sizeof(uint32_t);
                kernelInfo.workloadInfo.enqueuedLocalWorkSizeOffsets[index] =
                    pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_GLOBAL_WORK_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "GLOBAL_WORK_SIZE");
                index = pDataParameterBuffer->SourceOffset / sizeof(uint32_t);
                kernelInfo.workloadInfo.globalWorkSizeOffsets[index] =
                    pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_NUM_WORK_GROUPS:
                DBG_LOG(LogPatchTokens, "\n  .Type", "NUM_WORK_GROUPS");
                index = pDataParameterBuffer->SourceOffset / sizeof(uint32_t);
                kernelInfo.workloadInfo.numWorkGroupsOffset[index] =
                    pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_MAX_WORKGROUP_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "MAX_WORKGROUP_SIZE");
                kernelInfo.workloadInfo.maxWorkGroupSizeOffset = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_WORK_DIMENSIONS:
                DBG_LOG(LogPatchTokens, "\n  .Type", "WORK_DIMENSIONS");
                kernelInfo.workloadInfo.workDimOffset = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES: {
                DBG_LOG(LogPatchTokens, "\n  .Type", "SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);

                KernelArgPatchInfo kernelArgPatchInfo;
                kernelArgPatchInfo.size = pDataParameterBuffer->DataSize;
                kernelArgPatchInfo.crossthreadOffset = pDataParameterBuffer->Offset;

                kernelInfo.kernelArgInfo[argNum].slmAlignment = pDataParameterBuffer->SourceOffset;
                kernelInfo.kernelArgInfo[argNum].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
            } break;

            case DATA_PARAMETER_IMAGE_WIDTH:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_WIDTH");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetImgWidth = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_IMAGE_HEIGHT:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_HEIGHT");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetImgHeight = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_IMAGE_DEPTH:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_DEPTH");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetImgDepth = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED:
                DBG_LOG(LogPatchTokens, "\n  .Type", "SAMPLER_COORDINATE_SNAP_WA_REQUIRED");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetSamplerSnapWa = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_SAMPLER_ADDRESS_MODE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "SAMPLER_ADDRESS_MODE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetSamplerAddressingMode = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS:
                DBG_LOG(LogPatchTokens, "\n  .Type", "SAMPLER_ADDRESS_MODE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetSamplerNormalizedCoords = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "SAMPLER_ADDRESS_MODE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetChannelDataType = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_IMAGE_CHANNEL_ORDER:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_CHANNEL_ORDER");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetChannelOrder = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_IMAGE_ARRAY_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_ARRAY_SIZE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetArraySize = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_OBJECT_ID:
                DBG_LOG(LogPatchTokens, "\n  .Type", "OBJECT_ID");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetObjectId = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_SIMD_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "SIMD_SIZE");
                kernelInfo.workloadInfo.simdSizeOffset = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_PARENT_EVENT:
                DBG_LOG(LogPatchTokens, "\n  .Type", "PARENT_EVENT");
                kernelInfo.workloadInfo.parentEventOffset = pDataParameterBuffer->Offset;
                break;

            case DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "CHILD_BLOCK_SIMD_SIZE");
                kernelInfo.childrenKernelsIdOffset.push_back({argNum, pDataParameterBuffer->Offset});
                break;

            case DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "PRIVATE_MEMORY_STATELESS_SIZE");
                PrivateMemoryStatelessSizeOffset = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "LOCAL_MEMORY_STATELESS_WINDOW_SIZE");
                LocalMemoryStatelessWindowSizeOffset = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS:
                DBG_LOG(LogPatchTokens, "\n  .Type", "LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS");
                LocalMemoryStatelessWindowStartAddressOffset = pDataParameterBuffer->Offset;
                pDevice->prepareSLMWindow();
                break;
            case DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "PREFERRED_WORKGROUP_MULTIPLE");
                kernelInfo.workloadInfo.prefferedWkgMultipleOffset = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_BUFFER_OFFSET:
                DBG_LOG(LogPatchTokens, "\n  .Type", "DATA_PARAMETER_BUFFER_OFFSET");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetBufferOffset = pDataParameterBuffer->Offset;
                break;
            case DATA_PARAMETER_NUM_HARDWARE_THREADS:
            case DATA_PARAMETER_PRINTF_SURFACE_SIZE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "Unhandled", pDataParameterBuffer->Type);
                printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr,
                                 "Program::parsePatchList.Unhandled Data parameter: %d\n", pDataParameterBuffer->Type);
                break;

            case DATA_PARAMETER_VME_MB_BLOCK_TYPE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "VME_MB_BLOCK_TYPE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetVmeMbBlockType = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;
            case DATA_PARAMETER_VME_SUBPIXEL_MODE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "VME_SUBPIXEL_MODE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetVmeSubpixelMode = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;
            case DATA_PARAMETER_VME_SAD_ADJUST_MODE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "VME_SAD_ADJUST_MODE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetVmeSadAdjustMode = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;
            case DATA_PARAMETER_VME_SEARCH_PATH_TYPE:
                DBG_LOG(LogPatchTokens, "\n  .Type", "VME_SEARCH_PATH_TYPE");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetVmeSearchPathType = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;
            case DATA_PARAMETER_IMAGE_NUM_SAMPLES:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_NUM_SAMPLES");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetNumSamples = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;

            case DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS:
                DBG_LOG(LogPatchTokens, "\n  .Type", "IMAGE_NUM_MIP_LEVELS");
                kernelInfo.resizeKernelArgInfoAndRegisterParameter(argNum);
                kernelInfo.kernelArgInfo[argNum].offsetNumMipLevels = pDataParameterBuffer->Offset;
                DEBUG_BREAK_IF(pDataParameterBuffer->DataSize != sizeof(uint32_t));
                break;
            case DATA_PARAMETER_IMAGE_SRGB_CHANNEL_ORDER:
            case DATA_PARAMETER_STAGE_IN_GRID_ORIGIN:
            case DATA_PARAMETER_STAGE_IN_GRID_SIZE:
                break;

            case DATA_PARAMETER_LOCAL_ID:
            case DATA_PARAMETER_EXECUTION_MASK:
            case DATA_PARAMETER_VME_IMAGE_TYPE:
            case DATA_PARAMETER_VME_MB_SKIP_BLOCK_TYPE:
                break;

            default:
                kernelInfo.patchInfo.dataParameterBuffers.pop_back();

                DBG_LOG(LogPatchTokens, "\n  .Type", "Unhandled", pDataParameterBuffer->Type);
                DEBUG_BREAK_IF(true);
            }

            DBG_LOG(LogPatchTokens,
                    "\n  .ArgumentNumber", pDataParameterBuffer->ArgumentNumber,
                    "\n  .Offset", pDataParameterBuffer->Offset,
                    "\n  .DataSize", pDataParameterBuffer->DataSize,
                    "\n  .SourceOffset", pDataParameterBuffer->SourceOffset);

            break;

        case PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD:
            kernelInfo.patchInfo.interfaceDescriptorDataLoad =
                reinterpret_cast<const SPatchMediaInterfaceDescriptorLoad *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.MEDIA_INTERFACE_DESCRIPTOR_LOAD", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .InterfaceDescriptorDataOffset", kernelInfo.patchInfo.interfaceDescriptorDataLoad->InterfaceDescriptorDataOffset);
            break;

        case PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA:
            kernelInfo.patchInfo.interfaceDescriptorData =
                reinterpret_cast<const SPatchInterfaceDescriptorData *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.INTERFACE_DESCRIPTOR_DATA", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .Offset", kernelInfo.patchInfo.interfaceDescriptorData->Offset,
                    "\n  .SamplerStateOffset", kernelInfo.patchInfo.interfaceDescriptorData->SamplerStateOffset,
                    "\n  .KernelOffset", kernelInfo.patchInfo.interfaceDescriptorData->KernelOffset,
                    "\n  .BindingTableOffset", kernelInfo.patchInfo.interfaceDescriptorData->BindingTableOffset);
            break;

        case PATCH_TOKEN_THREAD_PAYLOAD:
            kernelInfo.patchInfo.threadPayload =
                reinterpret_cast<const SPatchThreadPayload *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.THREAD_PAYLOAD", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .HeaderPresent", kernelInfo.patchInfo.threadPayload->HeaderPresent,
                    "\n  .LocalIDXPresent", kernelInfo.patchInfo.threadPayload->LocalIDXPresent,
                    "\n  .LocalIDYPresent", kernelInfo.patchInfo.threadPayload->LocalIDYPresent,
                    "\n  .LocalIDZPresent", kernelInfo.patchInfo.threadPayload->LocalIDZPresent,
                    "\n  .LocalIDFlattenedPresent", kernelInfo.patchInfo.threadPayload->LocalIDFlattenedPresent,
                    "\n  .IndirectPayloadStorage", kernelInfo.patchInfo.threadPayload->IndirectPayloadStorage,
                    "\n  .UnusedPerThreadConstantPresent", kernelInfo.patchInfo.threadPayload->UnusedPerThreadConstantPresent,
                    "\n  .GetLocalIDPresent", kernelInfo.patchInfo.threadPayload->GetLocalIDPresent,
                    "\n  .GetGroupIDPresent", kernelInfo.patchInfo.threadPayload->GetGroupIDPresent,
                    "\n  .GetGlobalOffsetPresent", kernelInfo.patchInfo.threadPayload->GetGlobalOffsetPresent);
            break;

        case PATCH_TOKEN_EXECUTION_ENVIRONMENT:
            kernelInfo.patchInfo.executionEnvironment =
                reinterpret_cast<const SPatchExecutionEnvironment *>(pPatch);
            if (kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeX != 0) {
                kernelInfo.reqdWorkGroupSize[0] = kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
                kernelInfo.reqdWorkGroupSize[1] = kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
                kernelInfo.reqdWorkGroupSize[2] = kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;
                DEBUG_BREAK_IF(!(kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeY > 0));
                DEBUG_BREAK_IF(!(kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeZ > 0));
            }
            if (kernelInfo.patchInfo.executionEnvironment->CompiledForGreaterThan4GBBuffers == false) {
                kernelInfo.requiresSshForBuffers = true;
            }
            DBG_LOG(LogPatchTokens,
                    "\n.EXECUTION_ENVIRONMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .RequiredWorkGroupSizeX", kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeX,
                    "\n  .RequiredWorkGroupSizeY", kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeY,
                    "\n  .RequiredWorkGroupSizeZ", kernelInfo.patchInfo.executionEnvironment->RequiredWorkGroupSizeZ,
                    "\n  .LargestCompiledSIMDSize", kernelInfo.patchInfo.executionEnvironment->LargestCompiledSIMDSize,
                    "\n  .CompiledSubGroupsNumber", kernelInfo.patchInfo.executionEnvironment->CompiledSubGroupsNumber,
                    "\n  .HasBarriers", kernelInfo.patchInfo.executionEnvironment->HasBarriers,
                    "\n  .DisableMidThreadPreemption", kernelInfo.patchInfo.executionEnvironment->DisableMidThreadPreemption,
                    "\n  .CompiledSIMD8", kernelInfo.patchInfo.executionEnvironment->CompiledSIMD8,
                    "\n  .CompiledSIMD16", kernelInfo.patchInfo.executionEnvironment->CompiledSIMD16,
                    "\n  .CompiledSIMD32", kernelInfo.patchInfo.executionEnvironment->CompiledSIMD32,
                    "\n  .HasDeviceEnqueue", kernelInfo.patchInfo.executionEnvironment->HasDeviceEnqueue,
                    "\n  .MayAccessUndeclaredResource", kernelInfo.patchInfo.executionEnvironment->MayAccessUndeclaredResource,
                    "\n  .UsesFencesForReadWriteImages", kernelInfo.patchInfo.executionEnvironment->UsesFencesForReadWriteImages,
                    "\n  .UsesStatelessSpillFill", kernelInfo.patchInfo.executionEnvironment->UsesStatelessSpillFill,
                    "\n  .IsCoherent", kernelInfo.patchInfo.executionEnvironment->IsCoherent,
                    "\n  .SubgroupIndependentForwardProgressRequired", kernelInfo.patchInfo.executionEnvironment->SubgroupIndependentForwardProgressRequired);
            break;

        case PATCH_TOKEN_DATA_PARAMETER_STREAM:
            kernelInfo.patchInfo.dataParameterStream =
                reinterpret_cast<const SPatchDataParameterStream *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.DATA_PARAMETER_STREAM", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .DataParameterStreamSize", kernelInfo.patchInfo.dataParameterStream->DataParameterStreamSize);
            break;

        case PATCH_TOKEN_KERNEL_ARGUMENT_INFO: {
            auto pkernelArgInfo = reinterpret_cast<const SPatchKernelArgumentInfo *>(pPatch);
            kernelInfo.storeArgInfo(pkernelArgInfo);
            DBG_LOG(LogPatchTokens,
                    "\n.KERNEL_ARGUMENT_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pkernelArgInfo->ArgumentNumber,
                    "\n  .AddressQualifierSize", pkernelArgInfo->AddressQualifierSize,
                    "\n  .AccessQualifierSize", pkernelArgInfo->AccessQualifierSize,
                    "\n  .ArgumentNameSize", pkernelArgInfo->ArgumentNameSize,
                    "\n  .TypeNameSize", pkernelArgInfo->TypeNameSize,
                    "\n  .TypeQualifierSize", pkernelArgInfo->TypeQualifierSize);
            break;
        }

        case PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO:
            kernelInfo.patchInfo.pKernelAttributesInfo =
                reinterpret_cast<const SPatchKernelAttributesInfo *>(pPatch);
            kernelInfo.storePatchToken(kernelInfo.patchInfo.pKernelAttributesInfo);
            DBG_LOG(LogPatchTokens,
                    "\n.KERNEL_ATTRIBUTES_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .AttributesSize", kernelInfo.patchInfo.pKernelAttributesInfo->AttributesSize);
            break;

        case PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT: {
            const SPatchSamplerKernelArgument *pSamplerKernelObjectKernelArg = nullptr;

            pSamplerKernelObjectKernelArg = reinterpret_cast<const SPatchSamplerKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pSamplerKernelObjectKernelArg);
            DBG_LOG(LogPatchTokens,
                    "\n.SAMPLER_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pSamplerKernelObjectKernelArg->ArgumentNumber,
                    "\n  .Type", pSamplerKernelObjectKernelArg->Type,
                    "\n  .Offset", pSamplerKernelObjectKernelArg->Offset);
        };
            break;

        case PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT: {
            const SPatchImageMemoryObjectKernelArgument *pImageMemObjectKernelArg = nullptr;

            pImageMemObjectKernelArg =
                reinterpret_cast<const SPatchImageMemoryObjectKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pImageMemObjectKernelArg);
            DBG_LOG(LogPatchTokens,
                    "\n.IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pImageMemObjectKernelArg->ArgumentNumber,
                    "\n  .Type", pImageMemObjectKernelArg->Type,
                    "\n  .Offset", pImageMemObjectKernelArg->Offset,
                    "\n  .LocationIndex", pImageMemObjectKernelArg->LocationIndex,
                    "\n  .LocationIndex2", pImageMemObjectKernelArg->LocationIndex2,
                    "\n  .Transformable", pImageMemObjectKernelArg->Transformable);
        };
            break;

        case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT: {
            const SPatchGlobalMemoryObjectKernelArgument *pGlobalMemObjectKernelArg = nullptr;
            pGlobalMemObjectKernelArg =
                reinterpret_cast<const SPatchGlobalMemoryObjectKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pGlobalMemObjectKernelArg);
            DBG_LOG(LogPatchTokens,
                    "\n.GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pGlobalMemObjectKernelArg->ArgumentNumber,
                    "\n  .Offset", pGlobalMemObjectKernelArg->Offset,
                    "\n  .LocationIndex", pGlobalMemObjectKernelArg->LocationIndex,
                    "\n  .LocationIndex2", pGlobalMemObjectKernelArg->LocationIndex2);
        };
            break;

        case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT: {
            const SPatchStatelessGlobalMemoryObjectKernelArgument *pStatelessGlobalMemObjKernelArg = nullptr;

            pStatelessGlobalMemObjKernelArg =
                reinterpret_cast<const SPatchStatelessGlobalMemoryObjectKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pStatelessGlobalMemObjKernelArg);
            DBG_LOG(LogPatchTokens,
                    "\n.STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pStatelessGlobalMemObjKernelArg->ArgumentNumber,
                    "\n  .SurfaceStateHeapOffset", pStatelessGlobalMemObjKernelArg->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pStatelessGlobalMemObjKernelArg->DataParamOffset,
                    "\n  .DataParamSize", pStatelessGlobalMemObjKernelArg->DataParamSize,
                    "\n  .LocationIndex", pStatelessGlobalMemObjKernelArg->LocationIndex,
                    "\n  .LocationIndex2", pStatelessGlobalMemObjKernelArg->LocationIndex2);
        };
            break;

        case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT: {
            const SPatchStatelessConstantMemoryObjectKernelArgument *pPatchToken = reinterpret_cast<const SPatchStatelessConstantMemoryObjectKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pPatchToken->ArgumentNumber,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT: {
            const SPatchStatelessDeviceQueueKernelArgument *pPatchToken = reinterpret_cast<const SPatchStatelessDeviceQueueKernelArgument *>(pPatch);
            kernelInfo.storeKernelArgument(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ArgumentNumber", pPatchToken->ArgumentNumber,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY: {
            const SPatchAllocateStatelessPrivateSurface *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessPrivateSurface *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_PRIVATE_MEMORY", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize,
                    "\n  .PerThreadPrivateMemorySize", pPatchToken->PerThreadPrivateMemorySize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION: {
            const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ConstantBufferIndex", pPatchToken->ConstantBufferIndex,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION: {
            const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .GlobalBufferIndex", pPatchToken->GlobalBufferIndex,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE: {
            const SPatchAllocateStatelessPrintfSurface *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessPrintfSurface *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_PRINTF_SURFACE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .PrintfSurfaceIndex", pPatchToken->PrintfSurfaceIndex,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE: {
            const SPatchAllocateStatelessEventPoolSurface *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessEventPoolSurface *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_EVENT_POOL_SURFACE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .EventPoolSurfaceIndex", pPatchToken->EventPoolSurfaceIndex,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE: {
            const SPatchAllocateStatelessDefaultDeviceQueueSurface *pPatchToken = reinterpret_cast<const SPatchAllocateStatelessDefaultDeviceQueueSurface *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .SurfaceStateHeapOffset", pPatchToken->SurfaceStateHeapOffset,
                    "\n  .DataParamOffset", pPatchToken->DataParamOffset,
                    "\n  .DataParamSize", pPatchToken->DataParamSize);
        } break;

        case PATCH_TOKEN_STRING: {
            const SPatchString *pPatchToken = reinterpret_cast<const SPatchString *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.STRING", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .Index", pPatchToken->Index,
                    "\n  .StringSize", pPatchToken->StringSize);
        } break;

        case PATCH_TOKEN_INLINE_VME_SAMPLER_INFO:
            kernelInfo.isVmeWorkload = true;
            DBG_LOG(LogPatchTokens,
                    "\n.INLINE_VME_SAMPLER_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size);
            break;

        case PATCH_TOKEN_GTPIN_FREE_GRF_INFO: {
            const SPatchGtpinFreeGRFInfo *pPatchToken = reinterpret_cast<const SPatchGtpinFreeGRFInfo *>(pPatch);
            DBG_LOG(LogPatchTokens,
                    "\n.PATCH_TOKEN_GTPIN_FREE_GRF_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .BufferSize", pPatchToken->BufferSize);
        } break;

        case PATCH_TOKEN_STATE_SIP: {
            const SPatchStateSIP *pPatchToken = reinterpret_cast<const SPatchStateSIP *>(pPatch);
            kernelInfo.systemKernelOffset = pPatchToken->SystemKernelOffset;
            DBG_LOG(LogPatchTokens,
                    "\n.PATCH_TOKEN_STATE_SIP", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .SystemKernelOffset", pPatchToken->SystemKernelOffset);
        } break;

        case PATCH_TOKEN_ALLOCATE_SIP_SURFACE: {
            auto *pPatchToken = reinterpret_cast<const SPatchAllocateSystemThreadSurface *>(pPatch);
            kernelInfo.storePatchToken(pPatchToken);
            DBG_LOG(LogPatchTokens,
                    "\n.PATCH_TOKEN_ALLOCATE_SIP_SURFACE", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .BTI", pPatchToken->BTI,
                    "\n  .Offset", pPatchToken->Offset,
                    "\n  .PerThreadSystemThreadSurfaceSize", pPatchToken->PerThreadSystemThreadSurfaceSize);
        } break;

        default:
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, " Program::parsePatchList. Unknown Patch Token: %d\n", pPatch->Token);
            if (false == isSafeToSkipUnhandledToken(pPatch->Token)) {
                retVal = CL_INVALID_KERNEL;
            }
            break;
        }

        if (retVal != CL_SUCCESS) {
            break;
        }
        pCurPatchListPtr = ptrOffset(pCurPatchListPtr, pPatch->Size);
    }

    if (retVal == CL_SUCCESS) {
        retVal = kernelInfo.resolveKernelInfo();
    }

    if (kernelInfo.patchInfo.dataParameterStream && kernelInfo.patchInfo.dataParameterStream->DataParameterStreamSize) {
        uint32_t crossThreadDataSize = kernelInfo.patchInfo.dataParameterStream->DataParameterStreamSize;
        kernelInfo.crossThreadData = new char[crossThreadDataSize];
        memset(kernelInfo.crossThreadData, 0x00, crossThreadDataSize);

        if (LocalMemoryStatelessWindowStartAddressOffset != 0xFFffFFff) {
            *(uintptr_t *)&(kernelInfo.crossThreadData[LocalMemoryStatelessWindowStartAddressOffset]) = reinterpret_cast<uintptr_t>(this->pDevice->getSLMWindowStartAddress());
        }

        if (LocalMemoryStatelessWindowSizeOffset != 0xFFffFFff) {
            *(uint32_t *)&(kernelInfo.crossThreadData[LocalMemoryStatelessWindowSizeOffset]) = (uint32_t)this->pDevice->getDeviceInfo().localMemSize;
        }

        if (kernelInfo.patchInfo.pAllocateStatelessPrivateSurface && (PrivateMemoryStatelessSizeOffset != 0xFFffFFff)) {
            *(uint32_t *)&(kernelInfo.crossThreadData[PrivateMemoryStatelessSizeOffset]) = kernelInfo.patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize * this->getDevice(0).getDeviceInfo().computeUnitsUsedForScratch * kernelInfo.getMaxSimdSize();
        }

        if (kernelInfo.workloadInfo.maxWorkGroupSizeOffset != WorkloadInfo::undefinedOffset) {
            *(uint32_t *)&(kernelInfo.crossThreadData[kernelInfo.workloadInfo.maxWorkGroupSizeOffset]) = (uint32_t)this->getDevice(0).getDeviceInfo().maxWorkGroupSize;
        }
    }

    if (kernelInfo.heapInfo.pKernelHeader->KernelHeapSize && this->pDevice) {
        retVal = kernelInfo.createKernelAllocation(this->pDevice->getMemoryManager()) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
    }

    DEBUG_BREAK_IF(kernelInfo.heapInfo.pKernelHeader->KernelHeapSize && !this->pDevice);

    return retVal;
}

cl_int Program::parseProgramScopePatchList() {
    cl_int retVal = CL_SUCCESS;
    cl_uint surfaceSize = 0;

    auto pPatchList = programScopePatchList;
    auto patchListSize = programScopePatchListSize;
    auto pCurPatchListPtr = pPatchList;
    cl_uint headerSize = 0;

    while (ptrDiff(pCurPatchListPtr, pPatchList) < patchListSize) {
        auto pPatch = reinterpret_cast<const SPatchItemHeader *>(pCurPatchListPtr);
        switch (pPatch->Token) {
        case PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO: {
            auto patch = *(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo *)pPatch;

            if (constantSurface) {
                pDevice->getMemoryManager()->freeGraphicsMemory(constantSurface);
            }

            surfaceSize = patch.InlineDataSize;
            headerSize = sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo);

            constantSurface = pDevice->getMemoryManager()->createGraphicsAllocationWithRequiredBitness(surfaceSize, nullptr);

            memcpy_s(constantSurface->getUnderlyingBuffer(), surfaceSize, (cl_char *)pPatch + headerSize, surfaceSize);
            pCurPatchListPtr = ptrOffset(pCurPatchListPtr, surfaceSize);
            DBG_LOG(LogPatchTokens,
                    "\n  .ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .ConstantBufferIndex", patch.ConstantBufferIndex,
                    "\n  .InitializationDataSize", patch.InlineDataSize);
        };
            break;

        case PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO: {
            auto patch = *(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo *)pPatch;

            if (globalSurface) {
                pDevice->getMemoryManager()->freeGraphicsMemory(globalSurface);
            }

            surfaceSize = patch.InlineDataSize;
            globalVarTotalSize += (size_t)surfaceSize;
            headerSize = sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo);
            globalSurface = pDevice->getMemoryManager()->createGraphicsAllocationWithRequiredBitness(surfaceSize, nullptr);
            memcpy_s(globalSurface->getUnderlyingBuffer(), surfaceSize, (cl_char *)pPatch + headerSize, surfaceSize);
            pCurPatchListPtr = ptrOffset(pCurPatchListPtr, surfaceSize);
            DBG_LOG(LogPatchTokens,
                    "\n  .ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO", pPatch->Token,
                    "\n  .Size", pPatch->Size,
                    "\n  .BufferType", patch.Type,
                    "\n  .GlobalBufferIndex", patch.GlobalBufferIndex,
                    "\n  .InitializationDataSize", patch.InlineDataSize);
        };
            break;

        case PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO:
            if (globalSurface != nullptr) {
                auto patch = *(SPatchGlobalPointerProgramBinaryInfo *)pPatch;
                if ((patch.GlobalBufferIndex == 0) && (patch.BufferIndex == 0) && (patch.BufferType == PROGRAM_SCOPE_GLOBAL_BUFFER)) {
                    void *pPtr = (void *)((uintptr_t)globalSurface->getUnderlyingBuffer() + (uintptr_t)patch.GlobalPointerOffset);
                    if (globalSurface->is32BitAllocation) {
                        *reinterpret_cast<uint32_t *>(pPtr) += static_cast<uint32_t>(globalSurface->getGpuAddressToPatch());
                    } else {
                        *reinterpret_cast<uintptr_t *>(pPtr) += reinterpret_cast<uintptr_t>(globalSurface->getUnderlyingBuffer());
                    }
                } else {
                    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "Program::parseProgramScopePatchList. Unhandled Data parameter: %d\n", pPatch->Token);
                }
                DBG_LOG(LogPatchTokens,
                        "\n  .GLOBAL_POINTER_PROGRAM_BINARY_INFO", pPatch->Token,
                        "\n  .Size", pPatch->Size,
                        "\n  .GlobalBufferIndex", patch.GlobalBufferIndex,
                        "\n  .GlobalPointerOffset", patch.GlobalPointerOffset,
                        "\n  .BufferType", patch.BufferType,
                        "\n  .BufferIndex", patch.BufferIndex);
            }
            break;

        case PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO:
            if (constantSurface != nullptr) {
                auto patch = *(SPatchConstantPointerProgramBinaryInfo *)pPatch;
                if ((patch.ConstantBufferIndex == 0) && (patch.BufferIndex == 0) && (patch.BufferType == PROGRAM_SCOPE_CONSTANT_BUFFER)) {
                    void *pPtr = (uintptr_t *)((uintptr_t)constantSurface->getUnderlyingBuffer() + (uintptr_t)patch.ConstantPointerOffset);
                    if (constantSurface->is32BitAllocation) {
                        *reinterpret_cast<uint32_t *>(pPtr) += static_cast<uint32_t>(constantSurface->getGpuAddressToPatch());
                    } else {
                        *reinterpret_cast<uintptr_t *>(pPtr) += reinterpret_cast<uintptr_t>(constantSurface->getUnderlyingBuffer());
                    }

                } else {
                    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "Program::parseProgramScopePatchList. Unhandled Data parameter: %d\n", pPatch->Token);
                }
                DBG_LOG(LogPatchTokens,
                        "\n  .CONSTANT_POINTER_PROGRAM_BINARY_INFO", pPatch->Token,
                        "\n  .Size", pPatch->Size,
                        "\n  .ConstantBufferIndex", patch.ConstantBufferIndex,
                        "\n  .ConstantPointerOffset", patch.ConstantPointerOffset,
                        "\n  .BufferType", patch.BufferType,
                        "\n  .BufferIndex", patch.BufferIndex);
            }
            break;

        default:
            if (false == isSafeToSkipUnhandledToken(pPatch->Token)) {
                retVal = CL_INVALID_BINARY;
            }
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, " Program::parseProgramScopePatchList. Unknown Patch Token: %d\n", pPatch->Token);
            DBG_LOG(LogPatchTokens,
                    "\n  .Program Unknown Patch Token", pPatch->Token,
                    "\n  .Size", pPatch->Size);
        }

        if (retVal != CL_SUCCESS) {
            break;
        }
        pCurPatchListPtr = ptrOffset(pCurPatchListPtr, pPatch->Size);
    }

    return retVal;
}

cl_int Program::processGenBinary() {
    cl_int retVal = CL_SUCCESS;

    cleanCurrentKernelInfo();

    do {
        if (!genBinary || genBinarySize == 0) {
            retVal = CL_INVALID_BINARY;
            break;
        }

        auto pCurBinaryPtr = genBinary;
        auto pGenBinaryHeader = reinterpret_cast<const SProgramBinaryHeader *>(pCurBinaryPtr);
        if (!validateGenBinaryHeader(pGenBinaryHeader)) {
            retVal = CL_INVALID_BINARY;
            break;
        }

        pCurBinaryPtr = ptrOffset(pCurBinaryPtr, sizeof(SProgramBinaryHeader));
        programScopePatchList = pCurBinaryPtr;
        programScopePatchListSize = pGenBinaryHeader->PatchListSize;

        if (programScopePatchListSize != 0u) {
            retVal = parseProgramScopePatchList();
        }

        pCurBinaryPtr = ptrOffset(pCurBinaryPtr, pGenBinaryHeader->PatchListSize);

        auto numKernels = pGenBinaryHeader->NumberOfKernels;
        for (uint32_t i = 0; i < numKernels && retVal == CL_SUCCESS; i++) {

            size_t bytesProcessed = processKernel(pCurBinaryPtr, retVal);
            pCurBinaryPtr = ptrOffset(pCurBinaryPtr, bytesProcessed);
        }
    } while (false);

    return retVal;
}

bool Program::validateGenBinaryDevice(GFXCORE_FAMILY device) const {
    bool isValid = familyEnabled[device];

    return isValid;
}

bool Program::validateGenBinaryHeader(const iOpenCL::SProgramBinaryHeader *pGenBinaryHeader) const {
    return pGenBinaryHeader->Magic == MAGIC_CL &&
           pGenBinaryHeader->Version == CURRENT_ICBE_VERSION &&
           validateGenBinaryDevice(static_cast<GFXCORE_FAMILY>(pGenBinaryHeader->Device));
}

void Program::processDebugData() {
    if (debugData != nullptr) {
        SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<SProgramDebugDataHeaderIGC *>(debugData);

        DEBUG_BREAK_IF(programDebugHeader->NumberOfKernels != kernelInfoArray.size());

        const SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(SProgramDebugDataHeaderIGC)));
        const char *kernelName = nullptr;
        const char *kernelDebugData = nullptr;

        for (uint32_t i = 0; i < programDebugHeader->NumberOfKernels; i++) {
            kernelName = reinterpret_cast<const char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));

            auto kernelInfo = kernelInfoArray[i];
            UNRECOVERABLE_IF(kernelInfo->name.compare(0, kernelInfo->name.size(), kernelName) != 0);

            kernelDebugData = ptrOffset(kernelName, kernelDebugHeader->KernelNameSize);

            kernelInfo->debugData.vIsa = kernelDebugData;
            kernelInfo->debugData.genIsa = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);
            kernelInfo->debugData.vIsaSize = kernelDebugHeader->SizeVisaDbgInBytes;
            kernelInfo->debugData.genIsaSize = kernelDebugHeader->SizeGenIsaDbgInBytes;

            kernelDebugData = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes + kernelDebugHeader->SizeGenIsaDbgInBytes);
            kernelDebugHeader = reinterpret_cast<const SKernelDebugDataHeaderIGC *>(kernelDebugData);
        }
    }
}

} // namespace OCLRT
