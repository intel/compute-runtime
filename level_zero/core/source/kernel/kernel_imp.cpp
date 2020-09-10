/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/arrayref.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/program/kernel_info.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler.h"

#include <memory>

namespace L0 {
enum class SamplerPatchValues : uint32_t {
    DefaultSampler = 0x00,
    AddressNone = 0x00,
    AddressClamp = 0x01,
    AddressClampToEdge = 0x02,
    AddressRepeat = 0x03,
    AddressMirroredRepeat = 0x04,
    AddressMirroredRepeat101 = 0x05,
    NormalizedCoordsFalse = 0x00,
    NormalizedCoordsTrue = 0x08
};

inline SamplerPatchValues getAddrMode(ze_sampler_address_mode_t addressingMode) {
    switch (addressingMode) {
    case ZE_SAMPLER_ADDRESS_MODE_REPEAT:
        return SamplerPatchValues::AddressRepeat;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        return SamplerPatchValues::AddressClampToEdge;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP:
        return SamplerPatchValues::AddressClamp;
    case ZE_SAMPLER_ADDRESS_MODE_NONE:
        return SamplerPatchValues::AddressNone;
    case ZE_SAMPLER_ADDRESS_MODE_MIRROR:
        return SamplerPatchValues::AddressMirroredRepeat;
    default:
        DEBUG_BREAK_IF(true);
    }
    return SamplerPatchValues::AddressNone;
}

KernelImmutableData::KernelImmutableData(L0::Device *l0device) : device(l0device) {}

KernelImmutableData::~KernelImmutableData() {
    if (nullptr != isaGraphicsAllocation) {
        this->getDevice()->getNEODevice()->getMemoryManager()->freeGraphicsMemory(&*isaGraphicsAllocation);
        isaGraphicsAllocation.release();
    }
    crossThreadDataTemplate.reset();
    if (nullptr != privateMemoryGraphicsAllocation) {
        this->getDevice()->getNEODevice()->getMemoryManager()->freeGraphicsMemory(&*privateMemoryGraphicsAllocation);
        privateMemoryGraphicsAllocation.release();
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

    auto kernelIsaSize = kernelInfo->heapInfo.KernelHeapSize;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), kernelIsaSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, device->getDeviceBitfield()});
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

    if (kernelInfo->heapInfo.SurfaceStateHeapSize != 0) {
        this->surfaceStateHeapSize = kernelInfo->heapInfo.SurfaceStateHeapSize;
        surfaceStateHeapTemplate.reset(new uint8_t[surfaceStateHeapSize]);

        memcpy_s(surfaceStateHeapTemplate.get(), surfaceStateHeapSize,
                 kernelInfo->heapInfo.pSsh, surfaceStateHeapSize);
    }

    if (kernelInfo->heapInfo.DynamicStateHeapSize != 0) {
        this->dynamicStateHeapSize = kernelInfo->heapInfo.DynamicStateHeapSize;
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
            {device->getRootDeviceIndex(), privateSurfaceSize, NEO::GraphicsAllocation::AllocationType::PRIVATE_SURFACE, device->getDeviceBitfield()}));

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
        module->getDevice()->getNEODevice()->getMemoryManager()->freeGraphicsMemory(printfBuffer);
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
    const NEO::KernelDescriptor &desc = kernelImmData->getDescriptor();
    uint32_t globalWorkSize[3] = {groupCountX * groupSize[0], groupCountY * groupSize[1],
                                  groupCountZ * groupSize[2]};
    auto dst = ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize);
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.globalWorkSize, globalWorkSize);

    uint32_t groupCount[3] = {groupCountX, groupCountY, groupCountZ};
    NEO::patchVecNonPointer(dst, desc.payloadMappings.dispatchTraits.numWorkGroups, groupCount);
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
        return ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION;
    }

    this->groupSize[0] = groupSizeX;
    this->groupSize[1] = groupSizeY;
    this->groupSize[2] = groupSizeZ;
    const NEO::KernelDescriptor &kernelDescriptor = kernelImmData->getDescriptor();

    auto simdSize = kernelDescriptor.kernelAttributes.simdSize;
    this->numThreadsPerThreadGroup = static_cast<uint32_t>((itemsInGroup + simdSize - 1u) / simdSize);
    patchWorkgroupSizeInCrossThreadData(groupSizeX, groupSizeY, groupSizeZ);

    auto remainderSimdLanes = itemsInGroup & (simdSize - 1u);
    threadExecutionMask = static_cast<uint32_t>(maxNBitValue(remainderSimdLanes));
    if (!threadExecutionMask) {
        threadExecutionMask = static_cast<uint32_t>(maxNBitValue((simdSize == 1) ? 32 : simdSize));
    }
    evaluateIfRequiresGenerationOfLocalIdsByRuntime(kernelDescriptor);

    if (kernelRequiresGenerationOfLocalIdsByRuntime) {
        auto grfSize = this->module->getDevice()->getHwInfo().capabilityTable.grfSize;
        uint32_t perThreadDataSizeForWholeThreadGroupNeeded =
            static_cast<uint32_t>(NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(
                simdSize, grfSize, numChannels, itemsInGroup));
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
                static_cast<uint16_t>(simdSize),
                std::array<uint16_t, 3>{{static_cast<uint16_t>(groupSizeX),
                                         static_cast<uint16_t>(groupSizeY),
                                         static_cast<uint16_t>(groupSizeZ)}},
                std::array<uint8_t, 3>{{0, 1, 2}},
                false, grfSize);
        }

        this->perThreadDataSize = perThreadDataSizeForWholeThreadGroup / numThreadsPerThreadGroup;
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

        NEO::WorkSizeInfo wsInfo(maxWorkGroupSize, kernelImmData->getDescriptor().kernelAttributes.flags.usesBarriers, simd, this->getSlmTotalSize(),
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

ze_result_t KernelImp::setIndirectAccess(ze_kernel_indirect_access_flags_t flags) {
    if (NEO::DebugManager.flags.DisableIndirectAccess.get() == 1) {
        return ZE_RESULT_SUCCESS;
    }

    if (flags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE) {
        this->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    }
    if (flags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST) {
        this->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    }
    if (flags & ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED) {
        this->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getIndirectAccess(ze_kernel_indirect_access_flags_t *flags) {
    *flags = 0;
    if (this->unifiedMemoryControls.indirectDeviceAllocationsAllowed) {
        *flags |= ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    }
    if (this->unifiedMemoryControls.indirectHostAllocationsAllowed) {
        *flags |= ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    }
    if (this->unifiedMemoryControls.indirectSharedAllocationsAllowed) {
        *flags |= ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getSourceAttributes(uint32_t *pSize, char **pString) {
    auto &desc = kernelImmData->getDescriptor();
    if (pString == nullptr) {
        *pSize = (uint32_t)desc.kernelMetadata.kernelLanguageAttributes.length() + 1;
    } else {
        strncpy_s(*pString, desc.kernelMetadata.kernelLanguageAttributes.length() + 1,
                  desc.kernelMetadata.kernelLanguageAttributes.c_str(),
                  desc.kernelMetadata.kernelLanguageAttributes.length() + 1);
    }
    return ZE_RESULT_SUCCESS;
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

ze_result_t KernelImp::setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    const auto val = argVal;

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
    if (nullptr == svmAllocsManager->getSVMAlloc(requestedAddress)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    uint32_t rootDeviceIndex = module->getDevice()->getRootDeviceIndex();
    NEO::GraphicsAllocation *alloc = svmAllocsManager->getSVMAlloc(requestedAddress)->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    auto gpuAddress = reinterpret_cast<uintptr_t>(requestedAddress);
    return setArgBufferWithAlloc(argIndex, gpuAddress, alloc);
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

    auto imageInfo = image->getImageInfo();

    auto clChannelType = getClChannelDataType(image->getImageDesc().format);
    auto clChannelOrder = getClChannelOrder(image->getImageDesc().format);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.imgWidth, imageInfo.imgDesc.imageWidth);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.imgHeight, imageInfo.imgDesc.imageHeight);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.imgDepth, imageInfo.imgDesc.imageDepth);
    NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.numSamples, imageInfo.imgDesc.numSamples);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.arraySize, imageInfo.imgDesc.imageArraySize);
    NEO::patchNonPointer<cl_channel_type>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.channelDataType, clChannelType);
    NEO::patchNonPointer<cl_channel_order>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.channelOrder, clChannelOrder);
    NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.numMipLevels, imageInfo.imgDesc.numMipLevels);

    auto pixelSize = imageInfo.surfaceFormat->ImageElementSizeInBytes;
    NEO::patchNonPointer<uint64_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.flatBaseOffset, image->getAllocation()->getGpuAddress());
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.flatWidth, (imageInfo.imgDesc.imageWidth * pixelSize) - 1u);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.flatHeight, (imageInfo.imgDesc.imageHeight * pixelSize) - 1u);
    NEO::patchNonPointer<size_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.flatPitch, imageInfo.imgDesc.imageRowPitch - 1u);

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::setArgSampler(uint32_t argIndex, size_t argSize, const void *argVal) {
    const auto &arg = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescSampler>();
    const auto sampler = Sampler::fromHandle(*static_cast<const ze_sampler_handle_t *>(argVal));
    sampler->copySamplerStateToDSH(dynamicStateHeapData.get(), dynamicStateHeapDataSize, arg.bindful);

    auto samplerDesc = sampler->getSamplerDesc();

    NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.samplerSnapWa, (samplerDesc.addressMode == ZE_SAMPLER_ADDRESS_MODE_CLAMP && samplerDesc.filterMode == ZE_SAMPLER_FILTER_MODE_NEAREST) ? std::numeric_limits<uint32_t>::max() : 0u);
    NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.samplerAddressingMode, static_cast<uint32_t>(getAddrMode(samplerDesc.addressMode)));
    NEO::patchNonPointer<uint32_t>(ArrayRef<uint8_t>(crossThreadData.get(), crossThreadDataSize), arg.metadataPayload.samplerNormalizedCoords, samplerDesc.isNormalized ? static_cast<uint32_t>(SamplerPatchValues::NormalizedCoordsTrue) : static_cast<uint32_t>(SamplerPatchValues::NormalizedCoordsFalse));

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getKernelName(size_t *pSize, char *pName) {
    size_t kernelNameSize = this->kernelImmData->getDescriptor().kernelMetadata.kernelName.size() + 1;
    if (0 == *pSize || nullptr == pName) {
        *pSize = kernelNameSize;
        return ZE_RESULT_SUCCESS;
    }

    *pSize = std::min(*pSize, kernelNameSize);
    strncpy_s(pName, *pSize,
              this->kernelImmData->getDescriptor().kernelMetadata.kernelName.c_str(), kernelNameSize);

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::getProperties(ze_kernel_properties_t *pKernelProperties) {

    pKernelProperties->requiredGroupSizeX = this->groupSize[0];
    pKernelProperties->requiredGroupSizeY = this->groupSize[1];
    pKernelProperties->requiredGroupSizeZ = this->groupSize[2];

    pKernelProperties->numKernelArgs =
        static_cast<uint32_t>(this->kernelImmData->getDescriptor().payloadMappings.explicitArgs.size());

    ModuleImp *moduleImp = reinterpret_cast<ModuleImp *>(this->module);
    NEO::KernelInfo *ki = nullptr;
    for (uint32_t i = 0; i < moduleImp->getTranslationUnit()->programInfo.kernelInfos.size(); i++) {
        ki = moduleImp->getTranslationUnit()->programInfo.kernelInfos[i];
        if (ki->name.compare(0, ki->name.size(), this->kernelImmData->getDescriptor().kernelMetadata.kernelName) == 0) {
            break;
        }
    }

    if (nullptr == ki) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    pKernelProperties->requiredNumSubGroups = static_cast<uint32_t>(ki->patchInfo.executionEnvironment->CompiledSubGroupsNumber);
    pKernelProperties->requiredSubgroupSize = static_cast<uint32_t>(ki->requiredSubGroupSize);
    pKernelProperties->maxSubgroupSize = ki->getMaxSimdSize();

    uint32_t maxKernelWorkGroupSize = static_cast<uint32_t>(this->module->getDevice()->getNEODevice()->getDeviceInfo().maxWorkGroupSize);
    uint32_t maxRequiredWorkGroupSize = static_cast<uint32_t>(ki->getMaxRequiredWorkGroupSize(maxKernelWorkGroupSize));
    uint32_t largestCompiledSIMDSize = static_cast<uint32_t>(ki->patchInfo.executionEnvironment->LargestCompiledSIMDSize);
    pKernelProperties->maxNumSubgroups = static_cast<uint32_t>(Math::divideAndRoundUp(maxRequiredWorkGroupSize, largestCompiledSIMDSize));

    pKernelProperties->localMemSize = static_cast<uint32_t>(moduleImp->getDevice()->getNEODevice()->getDeviceInfo().localMemSize);
    pKernelProperties->privateMemSize = ki->patchInfo.pAllocateStatelessPrivateSurface ? ki->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize
                                                                                       : 0;
    pKernelProperties->spillMemSize = ki->patchInfo.mediavfestate ? ki->patchInfo.mediavfestate->PerThreadScratchSpace
                                                                  : 0;
    memset(pKernelProperties->uuid.kid, 0, ZE_MAX_KERNEL_UUID_SIZE);
    memset(pKernelProperties->uuid.mid, 0, ZE_MAX_MODULE_UUID_SIZE);

    return ZE_RESULT_SUCCESS;
}

ze_result_t KernelImp::initialize(const ze_kernel_desc_t *desc) {
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
    for (auto &alloc : module->getImportedSymbolAllocations()) {
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
    auto kernel = static_cast<KernelImp *>(allocator(module));
    *res = kernel->initialize(desc);
    if (*res) {
        kernel->destroy();
        return nullptr;
    }
    return kernel;
}

bool KernelImp::hasIndirectAllocationsAllowed() const {
    return (unifiedMemoryControls.indirectDeviceAllocationsAllowed ||
            unifiedMemoryControls.indirectHostAllocationsAllowed ||
            unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

uint32_t KernelImp::getSlmTotalSize() const {
    return slmArgsTotalSize + getImmutableData()->getDescriptor().kernelAttributes.slmInlineSize;
}

NEO::GraphicsAllocation *KernelImp::getIsaAllocation() const {
    return getImmutableData()->getIsaGraphicsAllocation();
}

} // namespace L0
