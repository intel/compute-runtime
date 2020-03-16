/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/arrayref.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/program/kernel_info.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/image.h"
#include "level_zero/core/source/module.h"
#include "level_zero/core/source/module_imp.h"
#include "level_zero/core/source/printf_handler.h"
#include "level_zero/core/source/sampler.h"

#include <memory>

namespace L0 {

KernelImmutableData::KernelImmutableData(L0::Device *l0device) : device(l0device) {}

KernelImmutableData::~KernelImmutableData() {
    if (nullptr != isaGraphicsAllocation) {
        this->getDevice()->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(&*isaGraphicsAllocation);
        isaGraphicsAllocation.release();
    }
    crossThreadDataTemplate.reset();
    if (nullptr != privateMemoryGraphicsAllocation) {
        this->getDevice()->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(&*privateMemoryGraphicsAllocation);
        crossThreadDataTemplate.release();
    }
    surfaceStateHeapTemplate.reset();
    dynamicStateHeapTemplate.reset();
}

inline void patchWithImplicitSurface(ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> surfaceStateHeap,
                                     uintptr_t ptrToPatchInCrossThreadData, NEO::GraphicsAllocation &allocation,
                                     const NEO::ArgDescPointer &ptr, const NEO::Device &device) {
    if (false == crossThreadData.empty()) {
        NEO::patchPointer(crossThreadData, ptr, ptrToPatchInCrossThreadData);
    }

    if ((false == surfaceStateHeap.empty()) && (NEO::isValidOffset(ptr.bindful))) {
        auto surfaceState = surfaceStateHeap.begin() + ptr.bindful;
        void *addressToPatch = reinterpret_cast<void *>(allocation.getUnderlyingBuffer());
        size_t sizeToPatch = allocation.getUnderlyingBufferSize();
        NEO::Buffer::setSurfaceState(&device, surfaceState, sizeToPatch, addressToPatch, 0,
                                     &allocation, 0, 0);
    }
}

void KernelImmutableData::initialize(NEO::KernelInfo *kernelInfo, NEO::MemoryManager &memoryManager,
                                     const NEO::Device *device, uint32_t computeUnitsUsedForSratch,
                                     NEO::GraphicsAllocation *globalConstBuffer,
                                     NEO::GraphicsAllocation *globalVarBuffer) {
    UNRECOVERABLE_IF(kernelInfo == nullptr);
    this->kernelDescriptor = &kernelInfo->kernelDescriptor;

    auto kernelIsaSize = kernelInfo->heapInfo.pKernelHeader->KernelHeapSize;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), kernelIsaSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA});
    UNRECOVERABLE_IF(allocation == nullptr);
    if (kernelInfo->heapInfo.pKernelHeap != nullptr) {
        memoryManager.copyMemoryToAllocation(allocation, kernelInfo->heapInfo.pKernelHeap, kernelIsaSize);
    }
    isaGraphicsAllocation.reset(allocation);

    this->crossThreadDataSize = this->kernelDescriptor->kernelAttributes.crossThreadDataSize;

    ArrayRef<uint8_t> crossThredDataArrayRef;
    if (crossThreadDataSize != 0) {
        crossThreadDataTemplate.reset(new uint8_t[crossThreadDataSize]);

        if (kernelInfo->crossThreadData) {
            memcpy_s(crossThreadDataTemplate.get(), crossThreadDataSize,
                     kernelInfo->crossThreadData, crossThreadDataSize);
        } else {
            memset(crossThreadDataTemplate.get(), 0x00, crossThreadDataSize);
        }

        crossThredDataArrayRef = ArrayRef<uint8_t>(this->crossThreadDataTemplate.get(), this->crossThreadDataSize);

        NEO::patchNonPointer<uint32_t>(crossThredDataArrayRef,
                                       kernelDescriptor->payloadMappings.implicitArgs.simdSize, kernelDescriptor->kernelAttributes.simdSize);
    }

    if (kernelInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize != 0) {
        this->surfaceStateHeapSize = kernelInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize;
        surfaceStateHeapTemplate.reset(new uint8_t[surfaceStateHeapSize]);

        memcpy_s(surfaceStateHeapTemplate.get(), surfaceStateHeapSize,
                 kernelInfo->heapInfo.pSsh, surfaceStateHeapSize);
    }

    if (kernelInfo->heapInfo.pKernelHeader->DynamicStateHeapSize != 0) {
        this->dynamicStateHeapSize = kernelInfo->heapInfo.pKernelHeader->DynamicStateHeapSize;
        dynamicStateHeapTemplate.reset(new uint8_t[dynamicStateHeapSize]);

        memcpy_s(dynamicStateHeapTemplate.get(), dynamicStateHeapSize,
                 kernelInfo->heapInfo.pDsh, dynamicStateHeapSize);
    }

    ArrayRef<uint8_t> surfaceStateHeapArrayRef = ArrayRef<uint8_t>(surfaceStateHeapTemplate.get(), getSurfaceStateHeapSize());
    uint32_t privateSurfaceSize = kernelDescriptor->kernelAttributes.perThreadPrivateMemorySize;

    if (privateSurfaceSize != 0) {
        privateSurfaceSize *= computeUnitsUsedForSratch * kernelDescriptor->kernelAttributes.simdSize;
        UNRECOVERABLE_IF(privateSurfaceSize == 0);
        this->privateMemoryGraphicsAllocation.reset(memoryManager.allocateGraphicsMemoryWithProperties(
            {0, privateSurfaceSize, NEO::GraphicsAllocation::AllocationType::PRIVATE_SURFACE}));

        UNRECOVERABLE_IF(this->privateMemoryGraphicsAllocation == nullptr);
        patchWithImplicitSurface(crossThredDataArrayRef, surfaceStateHeapArrayRef,
                                 static_cast<uintptr_t>(privateMemoryGraphicsAllocation->getGpuAddressToPatch()),
                                 *privateMemoryGraphicsAllocation, kernelDescriptor->payloadMappings.implicitArgs.privateMemoryAddress, *device);
        this->residencyContainer.push_back(this->privateMemoryGraphicsAllocation.get());
    }

    if (NEO::isValidOffset(kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless)) {
        UNRECOVERABLE_IF(nullptr == globalConstBuffer);

        patchWithImplicitSurface(crossThredDataArrayRef, surfaceStateHeapArrayRef,
                                 static_cast<uintptr_t>(globalConstBuffer->getGpuAddressToPatch()),
                                 *globalConstBuffer, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress, *device);
        this->residencyContainer.push_back(globalConstBuffer);
    }

    if (NEO::isValidOffset(kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless)) {
        UNRECOVERABLE_IF(globalVarBuffer == nullptr);

        patchWithImplicitSurface(crossThredDataArrayRef, surfaceStateHeapArrayRef,
                                 static_cast<uintptr_t>(globalVarBuffer->getGpuAddressToPatch()),
                                 *globalVarBuffer, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress, *device);
        this->residencyContainer.push_back(globalVarBuffer);
    }
}

uint32_t KernelImmutableData::getIsaSize() const {
    return static_cast<uint32_t>(isaGraphicsAllocation->getUnderlyingBufferSize());
}

uint64_t KernelImmutableData::getPrivateMemorySize() const {
    uint64_t size = 0;
    if (privateMemoryGraphicsAllocation != nullptr) {
        size = privateMemoryGraphicsAllocation->getUnderlyingBufferSize();
    }
    return size;
}

KernelImp::KernelImp(Module *module) : module(module) {}

KernelImp::~KernelImp() {
    if (perThreadDataForWholeThreadGroup != nullptr) {
        alignedFree(perThreadDataForWholeThreadGroup);
    }
    if (printfBuffer != nullptr) {
        module->getDevice()->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(printfBuffer);
    }
    slmArgSizes.clear();
    crossThreadData.reset();
    surfaceStateHeapData.reset();
    dynamicStateHeapData.reset();
}

ze_result_t KernelImp::setArgumentValue(uint32_t argIndex, size_t argSize,
                                        const void *pArgValue) {
    if (argIndex >= kernelArgHandlers.size()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return (this->*kernelArgHandlers[argIndex])(argIndex, argSize, pArgValue);
}

void KernelImp::setGroupCount(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    uint32_t groupSizeX;
    uint32_t groupSizeY;
    uint32_t groupSizeZ;
    getGroupSize(groupSizeX, groupSizeY, groupSizeZ);

    const NEO::KernelDescriptor &desc = kernelImmData->getDescriptor();
    uint32_t globalWorkSize[3] = {groupCountX * groupSizeX, groupCountY * groupSizeY,
                                  groupCountZ * groupSizeZ};
    auto dst = ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize);
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.globalWorkSize, globalWorkSize);

    uint32_t groupCount[3] = {groupCountX, groupCountY, groupCountZ};
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.numWorkGroups, groupCount);
}

bool KernelImp::getGroupCountOffsets(uint32_t *locations) {
    const NEO::KernelDescriptor &desc = kernelImmData->getDescriptor();
    for (int i = 0; i < 3; i++) {
        if (NEO::isValidOffset(desc.payloadMappings.dispatchTraits.numWorkGroups[i])) {
            locations[i] = desc.payloadMappings.dispatchTraits.numWorkGroups[i];
        } else {
            return false;
        }
    }
    return true;
}

bool KernelImp::getGroupSizeOffsets(uint32_t *locations) {
    const NEO::KernelDescriptor &desc = kernelImmData->getDescriptor();
    for (int i = 0; i < 3; i++) {
        if (NEO::isValidOffset(desc.payloadMappings.dispatchTraits.globalWorkSize[i])) {
            locations[i] = desc.payloadMappings.dispatchTraits.globalWorkSize[i];
        } else {
            return false;
        }
    }
    return true;
}

ze_result_t KernelImp::setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                                    uint32_t groupSizeZ) {
    if ((0 == groupSizeX) || (0 == groupSizeY) || (0 == groupSizeZ)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto numChannels = kernelImmData->getDescriptor().kernelAttributes.numLocalIdChannels;
    Vec3<size_t> groupSize{groupSizeX, groupSizeY, groupSizeZ};
    auto itemsInGroup = Math::computeTotalElementsCount(groupSize);

    if (itemsInGroup > module->getMaxGroupSize()) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    auto grfSize = kernelImmData->getDescriptor().kernelAttributes.grfSize;
    uint32_t perThreadDataSizeForWholeThreadGroupNeeded =
        static_cast<uint32_t>(NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(
            kernelImmData->getDescriptor().kernelAttributes.simdSize, grfSize, numChannels, itemsInGroup));
    if (perThreadDataSizeForWholeThreadGroupNeeded >
        perThreadDataSizeForWholeThreadGroupAllocated) {
        alignedFree(perThreadDataForWholeThreadGroup);
        perThreadDataForWholeThreadGroup = static_cast<uint8_t *>(alignedMalloc(perThreadDataSizeForWholeThreadGroupNeeded, 32));
        perThreadDataSizeForWholeThreadGroupAllocated = perThreadDataSizeForWholeThreadGroupNeeded;
    }
    perThreadDataSizeForWholeThreadGroup = perThreadDataSizeForWholeThreadGroupNeeded;

    if (numChannels > 0) {
        UNRECOVERABLE_IF(3 != numChannels);
        NEO::generateLocalIDs(
            perThreadDataForWholeThreadGroup,
            static_cast<uint16_t>(kernelImmData->getDescriptor().kernelAttributes.simdSize),
            std::array<uint16_t, 3>{{static_cast<uint16_t>(groupSizeX),
                                     static_cast<uint16_t>(groupSizeY),
                                     static_cast<uint16_t>(groupSizeZ)}},
            std::array<uint8_t, 3>{{0, 1, 2}},
            false, grfSize);
    }

    this->groupSize[0] = groupSizeX;
    this->groupSize[1] = groupSizeY;
    this->groupSize[2] = groupSizeZ;

    auto simdSize = kernelImmData->getDescriptor().kernelAttributes.simdSize;
    this->threadsPerThreadGroup = static_cast<uint32_t>((itemsInGroup + simdSize - 1u) / simdSize);
    this->perThreadDataSize = perThreadDataSizeForWholeThreadGroup / threadsPerThreadGroup;
    patchWorkgroupSizeInCrossThreadData(groupSizeX, groupSizeY, groupSizeZ);

    auto remainderSimdLanes = itemsInGroup & (simdSize - 1u);
    threadExecutionMask = static_cast<uint32_t>(maxNBitValue(remainderSimdLanes));
    if (!threadExecutionMask) {
        threadExecutionMask = ~threadExecutionMask;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                        uint32_t globalSizeZ, uint32_t *groupSizeX,
                                        uint32_t *groupSizeY, uint32_t *groupSizeZ) {
    size_t retGroupSize[3] = {};
    auto maxWorkGroupSize = module->getMaxGroupSize();
    auto simd = kernelImmData->getDescriptor().kernelAttributes.simdSize;
    size_t workItems[3] = {globalSizeX, globalSizeY, globalSizeZ};
    uint32_t dim = (globalSizeY > 1U) ? 2 : 1U;
    dim = (globalSizeZ > 1U) ? 3 : dim;

    if (NEO::DebugManager.flags.EnableComputeWorkSizeND.get()) {
        auto usesImages = getImmutableData()->getDescriptor().kernelAttributes.flags.usesImages;
        auto coreFamily = module->getDevice()->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
        const auto &deviceInfo = module->getDevice()->getNEODevice()->getDeviceInfo();
        uint32_t numThreadsPerSubSlice = (uint32_t)deviceInfo.maxNumEUsPerSubSlice * deviceInfo.numThreadsPerEU;
        uint32_t localMemSize = (uint32_t)deviceInfo.localMemSize;

        NEO::WorkSizeInfo wsInfo(maxWorkGroupSize, this->hasBarriers(), simd, this->getSlmTotalSize(),
                                 coreFamily, numThreadsPerSubSlice, localMemSize,
                                 usesImages, false);
        NEO::computeWorkgroupSizeND(wsInfo, retGroupSize, workItems, dim);
    } else {
        if (1U == dim) {
            NEO::computeWorkgroupSize1D(maxWorkGroupSize, retGroupSize, workItems, simd);
        } else if (NEO::DebugManager.flags.EnableComputeWorkSizeSquared.get() && (2U == dim)) {
            NEO::computeWorkgroupSizeSquared(maxWorkGroupSize, retGroupSize, workItems, simd, dim);
        } else {
            NEO::computeWorkgroupSize2D(maxWorkGroupSize, retGroupSize, workItems, simd);
        }
    }

    *groupSizeX = static_cast<uint32_t>(retGroupSize[0]);
    *groupSizeY = static_cast<uint32_t>(retGroupSize[1]);
    *groupSizeZ = static_cast<uint32_t>(retGroupSize[2]);

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::suggestMaxCooperativeGroupCount(uint32_t *totalGroupCount) {
    UNRECOVERABLE_IF(0 == groupSize[0]);
    UNRECOVERABLE_IF(0 == groupSize[1]);
    UNRECOVERABLE_IF(0 == groupSize[2]);

    auto &hardwareInfo = module->getDevice()->getHwInfo();

    auto dssCount = hardwareInfo.gtSystemInfo.DualSubSliceCount;
    if (dssCount == 0) {
        dssCount = hardwareInfo.gtSystemInfo.SubSliceCount;
    }
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto &descriptor = kernelImmData->getDescriptor();
    auto availableThreadCount = hwHelper.calculateAvailableThreadCount(
        hardwareInfo.platform.eProductFamily,
        descriptor.kernelAttributes.numGrfRequired,
        hardwareInfo.gtSystemInfo.EUCount, hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount);

    auto usesBarriers = descriptor.kernelAttributes.flags.usesBarriers;
    const uint32_t workDim = 3;
    const size_t localWorkSize[] = {groupSize[0], groupSize[1], groupSize[2]};
    *totalGroupCount = NEO::KernelHelper::getMaxWorkGroupCount(descriptor.kernelAttributes.simdSize,
                                                               availableThreadCount,
                                                               dssCount,
                                                               dssCount * KB * hardwareInfo.capabilityTable.slmSize,
                                                               hwHelper.alignSlmSize(slmArgsTotalSize + descriptor.kernelAttributes.slmInlineSize),
                                                               static_cast<uint32_t>(hwHelper.getMaxBarrierRegisterPerSlice()),
                                                               hwHelper.getBarriersCountFromHasBarriers(usesBarriers),
                                                               workDim,
                                                               localWorkSize);
    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setAttribute(ze_kernel_attribute_t attr, uint32_t size, const void *pValue) {
    if (size != sizeof(bool)) {
        return ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE;
    }

    if (attr == ZE_KERNEL_ATTR_INDIRECT_DEVICE_ACCESS) {
        this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = *(static_cast<const bool *>(pValue));
    } else if (attr == ZE_KERNEL_ATTR_INDIRECT_HOST_ACCESS) {
        this->unifiedMemoryControls.indirectHostAllocationsAllowed = *(static_cast<const bool *>(pValue));
    } else if (attr == ZE_KERNEL_ATTR_INDIRECT_SHARED_ACCESS) {
        this->unifiedMemoryControls.indirectSharedAllocationsAllowed = *(static_cast<const bool *>(pValue));
    } else {
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getAttribute(ze_kernel_attribute_t attr, uint32_t *pSize, void *pValue) {
    if (attr == ZE_KERNEL_ATTR_INDIRECT_DEVICE_ACCESS) {
        memcpy_s(pValue, sizeof(bool), &this->unifiedMemoryControls.indirectDeviceAllocationsAllowed, sizeof(bool));
        return ZE_RESULT_SUCCESS;
    }

    if (attr == ZE_KERNEL_ATTR_INDIRECT_HOST_ACCESS) {
        memcpy_s(pValue, sizeof(bool), &this->unifiedMemoryControls.indirectHostAllocationsAllowed, sizeof(bool));
        return ZE_RESULT_SUCCESS;
    }

    if (attr == ZE_KERNEL_ATTR_INDIRECT_SHARED_ACCESS) {
        memcpy_s(pValue, sizeof(bool), &this->unifiedMemoryControls.indirectSharedAllocationsAllowed, sizeof(bool));
        return ZE_RESULT_SUCCESS;
    }

    if (attr == ZE_KERNEL_ATTR_SOURCE_ATTRIBUTE) {
        auto &desc = kernelImmData->getDescriptor();
        if (pValue == nullptr) {
            *pSize = (uint32_t)desc.kernelMetadata.kernelLanguageAttributes.length() + 1;
        } else {
            strncpy_s((char *)pValue, desc.kernelMetadata.kernelLanguageAttributes.length() + 1,
                      desc.kernelMetadata.kernelLanguageAttributes.c_str(),
                      desc.kernelMetadata.kernelLanguageAttributes.length() + 1);
        }
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_INVALID_ENUMERATION;
}

ze_result_t KernelImp::setArgImmediate(uint32_t argIndex, size_t argSize, const void *argVal) {
    if (kernelImmData->getDescriptor().payloadMappings.explicitArgs.size() <= argIndex) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex];

    for (const auto &element : arg.as<NEO::ArgDescValue>().elements) {
        if (element.sourceOffset < argSize) {
            size_t maxBytesToCopy = argSize - element.sourceOffset;
            size_t bytesToCopy = std::min(static_cast<size_t>(element.size), maxBytesToCopy);

            auto pDst = ptrOffset(crossThreadData.get(), element.offset);
            if (argVal) {
                auto pSrc = ptrOffset(argVal, element.sourceOffset);
                memcpy_s(pDst, element.size, pSrc, bytesToCopy);
            } else {
                uint64_t val = 0;
                memcpy_s(pDst, element.size,
                         reinterpret_cast<void *>(&val), bytesToCopy);
            }
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescImage>();
    if (argVal == nullptr) {
        residencyContainer[argIndex] = nullptr;
        return ZE_RESULT_SUCCESS;
    }

    const auto image = Image::fromHandle(argVal);
    image->copyRedescribedSurfaceStateToSSH(surfaceStateHeapData.get(), arg.bindful);
    residencyContainer[argIndex] = image->getAllocation();

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setArgBufferWithAlloc(uint32_t argIndex, const void *argVal, NEO::GraphicsAllocation *allocation) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    const auto val = *reinterpret_cast<const uintptr_t *>(argVal);

    NEO::patchPointer(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg, val);
    if (NEO::isValidOffset(arg.bindful)) {
        setBufferSurfaceState(argIndex, reinterpret_cast<void *>(val), allocation);
    }
    residencyContainer[argIndex] = allocation;

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setArgBuffer(uint32_t argIndex, size_t argSize, const void *argVal) {
    const auto &allArgs = kernelImmData->getDescriptor().payloadMappings.explicitArgs;
    const auto &currArg = allArgs[argIndex];
    if (currArg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal) {
        slmArgSizes[argIndex] = static_cast<uint32_t>(argSize);
        UNRECOVERABLE_IF(NEO::isUndefinedOffset(currArg.as<NEO::ArgDescPointer>().slmOffset));
        auto slmOffset = *reinterpret_cast<uint32_t *>(crossThreadData.get() + currArg.as<NEO::ArgDescPointer>().slmOffset);
        slmOffset += static_cast<uint32_t>(argSize);
        ++argIndex;
        while (argIndex < kernelImmData->getDescriptor().payloadMappings.explicitArgs.size()) {
            if (allArgs[argIndex].getTraits().getAddressQualifier() != NEO::KernelArgMetadata::AddrLocal) {
                ++argIndex;
                continue;
            }
            const auto &nextArg = allArgs[argIndex].as<NEO::ArgDescPointer>();
            UNRECOVERABLE_IF(0 == nextArg.requiredSlmAlignment);
            slmOffset = alignUp<uint32_t>(slmOffset, nextArg.requiredSlmAlignment);
            NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), nextArg.slmOffset, slmOffset);

            slmOffset += static_cast<uint32_t>(slmArgSizes[argIndex]);
            ++argIndex;
        }
        slmArgsTotalSize = static_cast<uint32_t>(alignUp(slmOffset, KB));
        return ZE_RESULT_SUCCESS;
    }

    if (nullptr == argVal) {
        residencyContainer[argIndex] = nullptr;
        return ZE_RESULT_SUCCESS;
    }

    auto requestedAddress = *reinterpret_cast<void *const *>(argVal);
    auto svmAllocsManager = module->getDevice()->getDriverHandle()->getSvmAllocsManager();
    NEO::GraphicsAllocation *alloc = svmAllocsManager->getSVMAllocs()->get(requestedAddress)->gpuAllocation;

    return setArgBufferWithAlloc(argIndex, argVal, alloc);
}

ze_result_t KernelImp::setArgImage(uint32_t argIndex, size_t argSize, const void *argVal) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescImage>();
    if (argVal == nullptr) {
        residencyContainer[argIndex] = nullptr;
        return ZE_RESULT_SUCCESS;
    }

    const auto image = Image::fromHandle(*static_cast<const ze_image_handle_t *>(argVal));
    image->copySurfaceStateToSSH(surfaceStateHeapData.get(), arg.bindful);
    residencyContainer[argIndex] = image->getAllocation();

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setArgSampler(uint32_t argIndex, size_t argSize, const void *argVal) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescSampler>();
    const auto sampler = Sampler::fromHandle(*static_cast<const ze_sampler_handle_t *>(argVal));
    sampler->copySamplerStateToDSH(dynamicStateHeapData.get(), dynamicStateHeapDataSize, arg.bindful);
    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getProperties(ze_kernel_properties_t *pKernelProperties) {
    size_t kernel_name_size = std::min(this->kernelImmData->getDescriptor().kernelMetadata.kernelName.size(),
                                       static_cast<size_t>(ZE_MAX_KERNEL_NAME));
    strncpy_s(pKernelProperties->name, ZE_MAX_KERNEL_NAME,
              this->kernelImmData->getDescriptor().kernelMetadata.kernelName.c_str(), kernel_name_size);

    pKernelProperties->requiredGroupSizeX = this->groupSize[0];
    pKernelProperties->requiredGroupSizeY = this->groupSize[1];
    pKernelProperties->requiredGroupSizeZ = this->groupSize[2];

    pKernelProperties->numKernelArgs =
        static_cast<uint32_t>(this->kernelImmData->getDescriptor().payloadMappings.explicitArgs.size());

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::initialize(const ze_kernel_desc_t *desc) {
    if (desc->version != ZE_KERNEL_DESC_VERSION_CURRENT) {
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    this->kernelImmData = module->getKernelImmutableData(desc->pKernelName);
    if (this->kernelImmData == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (const auto &argT : kernelImmData->getDescriptor().payloadMappings.explicitArgs) {
        switch (argT.type) {
        default:
            UNRECOVERABLE_IF(true);
            break;
        case NEO::ArgDescriptor::ArgTPointer:
            this->kernelArgHandlers.push_back(&KernelImp::setArgBuffer);
            break;
        case NEO::ArgDescriptor::ArgTImage:
            this->kernelArgHandlers.push_back(&KernelImp::setArgImage);
            break;
        case NEO::ArgDescriptor::ArgTSampler:
            this->kernelArgHandlers.push_back(&KernelImp::setArgSampler);
            break;
        case NEO::ArgDescriptor::ArgTValue:
            this->kernelArgHandlers.push_back(&KernelImp::setArgImmediate);
            break;
        }
    }

    slmArgSizes.resize(this->kernelArgHandlers.size(), 0);

    if (kernelImmData->getSurfaceStateHeapSize() > 0) {
        this->surfaceStateHeapData.reset(new uint8_t[kernelImmData->getSurfaceStateHeapSize()]);
        memcpy_s(this->surfaceStateHeapData.get(),
                 kernelImmData->getSurfaceStateHeapSize(),
                 kernelImmData->getSurfaceStateHeapTemplate(),
                 kernelImmData->getSurfaceStateHeapSize());
        this->surfaceStateHeapDataSize = kernelImmData->getSurfaceStateHeapSize();
    }

    if (kernelImmData->getDescriptor().kernelAttributes.crossThreadDataSize != 0) {
        this->crossThreadData.reset(new uint8_t[kernelImmData->getDescriptor().kernelAttributes.crossThreadDataSize]);
        memcpy_s(this->crossThreadData.get(),
                 kernelImmData->getDescriptor().kernelAttributes.crossThreadDataSize,
                 kernelImmData->getCrossThreadDataTemplate(),
                 kernelImmData->getDescriptor().kernelAttributes.crossThreadDataSize);
        this->crossThreadDataSize = kernelImmData->getDescriptor().kernelAttributes.crossThreadDataSize;
    }

    if (kernelImmData->getDynamicStateHeapDataSize() != 0) {
        this->dynamicStateHeapData.reset(new uint8_t[kernelImmData->getDynamicStateHeapDataSize()]);
        memcpy_s(this->dynamicStateHeapData.get(),
                 kernelImmData->getDynamicStateHeapDataSize(),
                 kernelImmData->getDynamicStateHeapTemplate(),
                 kernelImmData->getDynamicStateHeapDataSize());
        this->dynamicStateHeapDataSize = kernelImmData->getDynamicStateHeapDataSize();
    }

    if (kernelImmData->getDescriptor().kernelAttributes.requiredWorkgroupSize[0] > 0) {
        auto *reqdSize = kernelImmData->getDescriptor().kernelAttributes.requiredWorkgroupSize;
        UNRECOVERABLE_IF(reqdSize[1] == 0);
        UNRECOVERABLE_IF(reqdSize[2] == 0);
        auto result = setGroupSize(reqdSize[0], reqdSize[1], reqdSize[2]);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    } else {
        auto result = setGroupSize(kernelImmData->getDescriptor().kernelAttributes.simdSize, 1, 1);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    residencyContainer.resize(this->kernelArgHandlers.size(), nullptr);

    this->createPrintfBuffer();

    this->setDebugSurface();

    for (auto &alloc : kernelImmData->getResidencyContainer()) {
        residencyContainer.push_back(alloc);
    }

    return ZE_RESULT_SUCCESS;
}

void KernelImp::createPrintfBuffer() {
    if (this->kernelImmData->getDescriptor().kernelAttributes.flags.usesPrintf) {
        this->printfBuffer = PrintfHandler::createPrintfBuffer(this->module->getDevice());
        this->residencyContainer.push_back(printfBuffer);
        NEO::patchPointer(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize),
                          this->getImmutableData()->getDescriptor().payloadMappings.implicitArgs.printfSurfaceAddress,
                          static_cast<uintptr_t>(this->printfBuffer->getGpuAddressToPatch()));
    }
}

void KernelImp::printPrintfOutput() {
    PrintfHandler::printOutput(kernelImmData, this->printfBuffer, module->getDevice());
}

void KernelImp::setDebugSurface() {
    auto device = module->getDevice();
    if (module->isDebugEnabled() && device->getNEODevice()->isDebuggerActive()) {

        auto surfaceStateHeapRef = ArrayRef<uint8_t>(surfaceStateHeapData.get(), surfaceStateHeapDataSize);

        patchWithImplicitSurface(ArrayRef<uint8_t>(), surfaceStateHeapRef,
                                 0,
                                 *device->getDebugSurface(), this->getImmutableData()->getDescriptor().payloadMappings.implicitArgs.systemThreadSurfaceAddress,
                                 *device->getNEODevice());
    }
}

void KernelImp::patchWorkgroupSizeInCrossThreadData(uint32_t x, uint32_t y, uint32_t z) {
    const NEO::KernelDescriptor &desc = kernelImmData->getDescriptor();
    auto dst = ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize);
    uint32_t workgroupSize[3] = {x, y, z};
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.localWorkSize, workgroupSize);
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.localWorkSize2, workgroupSize);
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.enqueuedLocalWorkSize, workgroupSize);
}

Kernel *Kernel::create(uint32_t productFamily, Module *module,
                       const ze_kernel_desc_t *desc, ze_result_t *res) {
    UNRECOVERABLE_IF(productFamily >= IGFX_MAX_PRODUCT);
    KernelAllocatorFn allocator = kernelFactory[productFamily];
    auto function = static_cast<KernelImp *>(allocator(module));
    *res = function->initialize(desc);
    if (*res) {
        function->destroy();
        return nullptr;
    }
    return function;
}

bool KernelImp::hasIndirectAllocationsAllowed() const {
    return (unifiedMemoryControls.indirectDeviceAllocationsAllowed ||
            unifiedMemoryControls.indirectHostAllocationsAllowed ||
            unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

bool KernelImp::hasBarriers() {
    return getImmutableData()->getDescriptor().kernelAttributes.flags.usesBarriers;
}
uint32_t KernelImp::getSlmTotalSize() {
    return slmArgsTotalSize + getImmutableData()->getDescriptor().kernelAttributes.slmInlineSize;
}
uint32_t KernelImp::getBindingTableOffset() {
    return getImmutableData()->getDescriptor().payloadMappings.bindingTable.tableOffset;
}
uint32_t KernelImp::getBorderColor() {
    return getImmutableData()->getDescriptor().payloadMappings.samplerTable.borderColor;
}
uint32_t KernelImp::getSamplerTableOffset() {
    return getImmutableData()->getDescriptor().payloadMappings.samplerTable.tableOffset;
}
uint32_t KernelImp::getNumSurfaceStates() {
    return getImmutableData()->getDescriptor().payloadMappings.bindingTable.numEntries;
}
uint32_t KernelImp::getNumSamplers() {
    return getImmutableData()->getDescriptor().payloadMappings.samplerTable.numSamplers;
}
uint32_t KernelImp::getSimdSize() {
    return getImmutableData()->getDescriptor().kernelAttributes.simdSize;
}
uint32_t KernelImp::getSizeCrossThreadData() {
    return getCrossThreadDataSize();
}
uint32_t KernelImp::getPerThreadScratchSize() {
    return getImmutableData()->getDescriptor().kernelAttributes.perThreadScratchSize[0];
}
uint32_t KernelImp::getThreadsPerThreadGroupCount() {
    return getThreadsPerThreadGroup();
}
uint32_t KernelImp::getSizePerThreadData() {
    return getPerThreadDataSize();
}
uint32_t KernelImp::getSizePerThreadDataForWholeGroup() {
    return getPerThreadDataSizeForWholeThreadGroup();
}
uint32_t KernelImp::getSizeSurfaceStateHeapData() {
    return getSurfaceStateHeapDataSize();
}
uint32_t KernelImp::getPerThreadExecutionMask() {
    return getThreadExecutionMask();
}
uint32_t *KernelImp::getCountOffsets() {
    return groupCountOffsets;
}
uint32_t *KernelImp::getSizeOffsets() {
    return groupSizeOffsets;
}
uint32_t *KernelImp::getLocalWorkSize() {
    if (hasGroupSize()) {
        getGroupSize(localWorkSize[0], localWorkSize[1], localWorkSize[2]);
    }
    return localWorkSize;
}
uint32_t KernelImp::getNumGrfRequired() {
    return getImmutableData()->getDescriptor().kernelAttributes.numGrfRequired;
}
NEO::GraphicsAllocation *KernelImp::getIsaAllocation() {
    return getImmutableData()->getIsaGraphicsAllocation();
}
bool KernelImp::hasGroupCounts() {
    return getGroupCountOffsets(groupCountOffsets);
}
bool KernelImp::hasGroupSize() {
    return getGroupSizeOffsets(groupSizeOffsets);
}
const void *KernelImp::getSurfaceStateHeap() {
    return getSurfaceStateHeapData();
}
const void *KernelImp::getDynamicStateHeap() {
    return getDynamicStateHeapData();
}
const void *KernelImp::getCrossThread() {
    return getCrossThreadData();
}
const void *KernelImp::getPerThread() {
    return getPerThreadData();
}

} // namespace L0
