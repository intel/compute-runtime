/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"

namespace L0::MCL {
using dword = uint32_t;

template <typename GfxFamily>
const bool MutableComputeWalkerHw<GfxFamily>::isHeapless = true;

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setKernelStartAddress(GpuAddress kernelStartAddress) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using InterfaceDescriptorData = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA_2;
    using KernelStartPointer = typename InterfaceDescriptorData::KERNELSTARTPOINTER;

    constexpr uint32_t kernelStartAddressIndex = 0;

    kernelStartAddress = (kernelStartAddress >> KernelStartPointer::KERNELSTARTPOINTER_BIT_SHIFT << KernelStartPointer::KERNELSTARTPOINTER_BIT_SHIFT);

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (cpuBufferWalker->getInterfaceDescriptor().getKernelStartPointer() == kernelStartAddress) {
        return;
    }
    cpuBufferWalker->getInterfaceDescriptor().setKernelStartPointer(kernelStartAddress);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        memcpy_s(&walkerCmd->getInterfaceDescriptor().getRawData(kernelStartAddressIndex), sizeof(GpuAddress), &kernelStartAddress, sizeof(GpuAddress));
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setIndirectDataStartAddress(GpuAddress indirectDataStartAddress) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto indirectDataStartAddressPatch = reinterpret_cast<uintptr_t>(cpuBufferWalker->getInlineDataPointer()) + this->indirectOffset;

    memcpy_s(reinterpret_cast<void *>(indirectDataStartAddressPatch), sizeof(GpuAddress), &indirectDataStartAddress, sizeof(GpuAddress));

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        auto indirectDataStartAddressPatch = reinterpret_cast<uintptr_t>(walkerCmd->getInlineDataPointer()) + this->indirectOffset;

        memcpy_s(reinterpret_cast<void *>(indirectDataStartAddressPatch), sizeof(GpuAddress), &indirectDataStartAddress, sizeof(GpuAddress));
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setIndirectDataSize(size_t indirectDataSize) {
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setBindingTablePointer(GpuAddress bindingTablePointer) {
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setGenerateLocalId(bool generateLocalIdsByGpu, uint32_t walkOrder, uint32_t localIdDimensions) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    constexpr uint32_t localIdGenerationIndex = 4;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (cpuBufferWalker->getGenerateLocalId() == generateLocalIdsByGpu &&
        cpuBufferWalker->getWalkOrder() == walkOrder) {
        return;
    }

    if (generateLocalIdsByGpu) {

        UNRECOVERABLE_IF(localIdDimensions > 3);
        uint32_t emitLocal = 0;
        if (localIdDimensions > 0) {
            emitLocal |= (1 << 0);
        }
        if (localIdDimensions > 1) {
            emitLocal |= (1 << 1);
        }
        if (localIdDimensions > 2) {
            emitLocal |= (1 << 2);
        }
        constexpr uint32_t generateLocalId = 0b1;

        cpuBufferWalker->setEmitLocalId(emitLocal);
        cpuBufferWalker->setGenerateLocalId(generateLocalId);
    } else {
        cpuBufferWalker->setEmitLocalId(0);
        cpuBufferWalker->setGenerateLocalId(0);
    }
    cpuBufferWalker->setWalkOrder(walkOrder);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getRawData(localIdGenerationIndex) = cpuBufferWalker->getRawData(localIdGenerationIndex);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setNumberThreadsPerThreadGroup(uint32_t numThreadPerThreadGroup) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    constexpr uint32_t numberThreadsPerThreadGroupIndex = 5;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &cpuBufferIdd = cpuBufferWalker->getInterfaceDescriptor();

    if (cpuBufferIdd.getNumberOfThreadsInGpgpuThreadGroup() == numThreadPerThreadGroup) {
        return;
    }
    cpuBufferIdd.setNumberOfThreadsInGpgpuThreadGroup(numThreadPerThreadGroup);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(numberThreadsPerThreadGroupIndex) = cpuBufferIdd.getRawData(numberThreadsPerThreadGroupIndex);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setNumberWorkGroups(MaxChannelsArray numberWorkGroups) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->setThreadGroupIdXDimension(numberWorkGroups[0]);
    cpuBufferWalker->setThreadGroupIdYDimension(numberWorkGroups[1]);
    cpuBufferWalker->setThreadGroupIdZDimension(numberWorkGroups[2]);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->setThreadGroupIdXDimension(numberWorkGroups[0]);
        walkerCmd->setThreadGroupIdYDimension(numberWorkGroups[1]);
        walkerCmd->setThreadGroupIdZDimension(numberWorkGroups[2]);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setExecutionMask(uint32_t mask) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (cpuBufferWalker->getExecutionMask() == mask) {
        return;
    }
    cpuBufferWalker->setExecutionMask(mask);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->setExecutionMask(mask);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setPostSyncAddress(GpuAddress postSyncAddress, GpuAddress inOrderIncrementAddress) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using PostSyncData = typename GfxFamily::POSTSYNC_DATA_2;

    using PostSyncPair = std::pair<PostSyncData *, PostSyncData *>;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    // <GPU, CPU> pointers
    const std::array<PostSyncPair, 4> postSyncTable = {{
        {&walkerCmd->getPostSyncOpn3(), &cpuBufferWalker->getPostSyncOpn3()},
        {&walkerCmd->getPostSyncOpn2(), &cpuBufferWalker->getPostSyncOpn2()},
        {&walkerCmd->getPostSyncOpn1(), &cpuBufferWalker->getPostSyncOpn1()},
        {&walkerCmd->getPostSync(), &cpuBufferWalker->getPostSync()},
    }};

    bool skipPostSyncAddress = (postSyncAddress == 0);
    bool skipInOrderIncrementAddress = (inOrderIncrementAddress == 0);

    for (auto &postSync : postSyncTable) {
        // Looping from the end. Increment address (if used) is always the last operation.
        auto addressToSet = skipInOrderIncrementAddress ? postSyncAddress : inOrderIncrementAddress;

        if (postSync.second->getDestinationAddress() != 0) {
            postSync.first->setDestinationAddress(addressToSet);
            postSync.second->setDestinationAddress(addressToSet);

            if (!skipInOrderIncrementAddress) {
                skipInOrderIncrementAddress = true;
            } else {
                skipPostSyncAddress = true;
            }
        }

        if (skipInOrderIncrementAddress && skipPostSyncAddress) {
            break;
        }
    }
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::getInlineDataPointer() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    return walkerCmd->getInlineDataPointer();
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::getHostMemoryInlineDataPointer() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);

    return cpuBufferWalker->getInlineDataPointer();
}

template <typename GfxFamily>
size_t MutableComputeWalkerHw<GfxFamily>::getInlineDataOffset() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    return offsetof(WalkerType, TheStructure.Common.InlineData[0]);
}

template <typename GfxFamily>
size_t MutableComputeWalkerHw<GfxFamily>::getInlineDataSize() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    return walkerCmd->getInlineDataSize();
}

template <typename GfxFamily>
size_t MutableComputeWalkerHw<GfxFamily>::getCommandSize() {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return sizeof(WalkerType);
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::createCommandBuffer() {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return new WalkerType;
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::deleteCommandBuffer() {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    delete (reinterpret_cast<WalkerType *>(cpuBuffer));
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::copyWalkerDataToHostBuffer(MutableComputeWalker *sourceWalker) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using PostSyncData = typename GfxFamily::POSTSYNC_DATA_2;

    auto sourceWalkerCmdHostBuffer = reinterpret_cast<WalkerType *>(static_cast<MutableComputeWalkerHw<GfxFamily> *>(sourceWalker)->cpuBuffer);
    auto walkerCmdHostBuffer = reinterpret_cast<WalkerType *>(this->cpuBuffer);

    memcpy_s(&walkerCmdHostBuffer->getPostSync(), sizeof(PostSyncData), &sourceWalkerCmdHostBuffer->getPostSync(), sizeof(PostSyncData));
    memcpy_s(&walkerCmdHostBuffer->getPostSyncOpn1(), sizeof(PostSyncData), &sourceWalkerCmdHostBuffer->getPostSyncOpn1(), sizeof(PostSyncData));
    memcpy_s(&walkerCmdHostBuffer->getPostSyncOpn2(), sizeof(PostSyncData), &sourceWalkerCmdHostBuffer->getPostSyncOpn2(), sizeof(PostSyncData));
    memcpy_s(&walkerCmdHostBuffer->getPostSyncOpn3(), sizeof(PostSyncData), &sourceWalkerCmdHostBuffer->getPostSyncOpn3(), sizeof(PostSyncData));

    auto indirectAddressSource = ptrOffset(sourceWalkerCmdHostBuffer->getInlineDataPointer(), sourceWalker->getIndirectOffset());
    auto indirectAddressThis = ptrOffset(walkerCmdHostBuffer->getInlineDataPointer(), this->indirectOffset);
    memcpy_s(indirectAddressThis, sizeof(uint64_t), indirectAddressSource, sizeof(uint64_t));
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::updateWalkerScratchPatchAddress(GpuAddress scratchPatchAddress) {
    if (isDefined(this->scratchOffset)) {
        using WalkerType = typename GfxFamily::DefaultWalkerType;
        auto walkerCmdHostBuffer = reinterpret_cast<WalkerType *>(this->cpuBuffer);
        auto scratchAddressThis = ptrOffset(walkerCmdHostBuffer->getInlineDataPointer(), this->scratchOffset);
        memcpy_s(scratchAddressThis, sizeof(uint64_t), &scratchPatchAddress, sizeof(uint64_t));
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::updateSpecificFields(const NEO::Device &device,
                                                             MutableWalkerSpecificFieldsArguments &args) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &idd = cpuBufferWalker->getInterfaceDescriptor();

    if (args.updateGroupCount || args.updateGroupSize) {
        NEO::EncodeDispatchKernel<GfxFamily>::encodeThreadGroupDispatch(idd, device, device.getHardwareInfo(),
                                                                        args.threadGroupDimensions, args.threadGroupCount, args.requiredThreadGroupDispatchSize,
                                                                        args.grfCount, args.threadsPerThreadGroup, *cpuBufferWalker);
        auto kernelExecutionType = args.cooperativeKernel ? NEO::KernelExecutionType::concurrent : NEO::KernelExecutionType::defaultType;

        NEO::EncodeWalkerArgs encodeWalkerArgs{
            .kernelExecutionType = kernelExecutionType,
            .requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none,
            .maxFrontEndThreads = device.getDeviceInfo().maxFrontEndThreads,
            .requiredSystemFence = false,
            .hasSample = false};

        NEO::EncodeDispatchKernel<GfxFamily>::encodeComputeDispatchAllWalker(*cpuBufferWalker, &idd, device.getRootDeviceEnvironment(), encodeWalkerArgs);
    }

    if (args.isSlmKernel && (args.updateGroupSize || args.updateSlm)) {
        NEO::EncodeDispatchKernel<GfxFamily>::setupPreferredSlmSize(&idd,
                                                                    device.getRootDeviceEnvironment(),
                                                                    args.threadsPerThreadGroup,
                                                                    args.threadGroupCount,
                                                                    args.slmTotalSize,
                                                                    static_cast<NEO::SlmPolicy>(args.slmPolicy));
    }

    if (args.updateGroupCount || args.updateGroupSize) {
        if (args.partitionCount > 1) {
            this->updateImplicitScalingData<WalkerType>(device, args.partitionCount, args.totalWorkGroupSize, args.threadGroupCount,
                                                        args.requiredPartitionDim, args.isRequiredDispatchWorkGroupOrder, args.cooperativeKernel);
        }
    }

    if (!this->stageCommitMode) {
        if (args.partitionCount == 1) {
            constexpr uint32_t dispatchWalkOrderIndex = 4;
            constexpr uint32_t quantumSizeIndex = 13;

            auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
            walkerCmd->getInterfaceDescriptor() = idd;

            walkerCmd->getRawData(dispatchWalkOrderIndex) = cpuBufferWalker->getRawData(dispatchWalkOrderIndex);
            walkerCmd->getRawData(quantumSizeIndex) = cpuBufferWalker->getRawData(quantumSizeIndex);
        } else {
            this->saveCpuBufferIntoGpuBuffer(true, false);
        }
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setSlmSize(uint32_t slmSize) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &cpuBufferIdd = cpuBufferWalker->getInterfaceDescriptor();

    if (cpuBufferIdd.getSharedLocalMemorySize() == slmSize) {
        return;
    }
    cpuBufferIdd.setSharedLocalMemorySize(slmSize);

    if (!this->stageCommitMode) {
        constexpr uint32_t slmSizeIddIndex = 5;

        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(slmSizeIddIndex) = cpuBufferIdd.getRawData(slmSizeIddIndex);
    }
}

} // namespace L0::MCL
