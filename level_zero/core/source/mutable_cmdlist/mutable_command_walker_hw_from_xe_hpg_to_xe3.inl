/*
 * Copyright (C) 2025 Intel Corporation
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
const bool MutableComputeWalkerHw<GfxFamily>::isHeapless = false;

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setKernelStartAddress(GpuAddress kernelStartAddress) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using InterfaceDescriptorData = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using KernelStartPointer = typename InterfaceDescriptorData::KERNELSTARTPOINTER;

    constexpr uint32_t kernelStartAddressIndex = 0;

    kernelStartAddress = (kernelStartAddress >> KernelStartPointer::KERNELSTARTPOINTER_BIT_SHIFT << KernelStartPointer::KERNELSTARTPOINTER_BIT_SHIFT);
    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (cpuBufferWalker->getInterfaceDescriptor().getKernelStartPointer() == kernelStartAddress) {
        return;
    }
    cpuBufferWalker->getInterfaceDescriptor().setKernelStartPointer(getLowPart(kernelStartAddress));

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(kernelStartAddressIndex) = getLowPart(kernelStartAddress);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setIndirectDataStartAddress(GpuAddress indirectDataStartAddress) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using IndirectDataStartAddress = typename WalkerType::INDIRECTDATASTARTADDRESS;

    constexpr uint32_t indirectDataStartAddressIndex = 3;

    UNRECOVERABLE_IF(indirectDataStartAddress > 0xffffffc0);

    indirectDataStartAddress = (indirectDataStartAddress >> IndirectDataStartAddress::INDIRECTDATASTARTADDRESS_BIT_SHIFT << IndirectDataStartAddress::INDIRECTDATASTARTADDRESS_BIT_SHIFT);

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->setIndirectDataStartAddress(getLowPart(indirectDataStartAddress));

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getRawData(indirectDataStartAddressIndex) = getLowPart(indirectDataStartAddress);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setIndirectDataSize(size_t indirectDataSize) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    constexpr uint32_t indirectDataSizeIndex = 2;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->setIndirectDataLength(static_cast<uint32_t>(indirectDataSize));

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getRawData(indirectDataSizeIndex) = cpuBufferWalker->getRawData(indirectDataSizeIndex);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setBindingTablePointer(GpuAddress bindingTablePointer) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    using InterfaceDescriptorData = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using BindingTablePointer = typename InterfaceDescriptorData::BINDINGTABLEPOINTER;

    constexpr uint32_t bindingTablePointerIndex = 4;

    bindingTablePointer = (bindingTablePointer >> BindingTablePointer::BINDINGTABLEPOINTER_BIT_SHIFT << BindingTablePointer::BINDINGTABLEPOINTER_BIT_SHIFT);

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &cpuBufferIdd = cpuBufferWalker->getInterfaceDescriptor();
    cpuBufferIdd.setBindingTablePointer(static_cast<uint32_t>(bindingTablePointer));

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(bindingTablePointerIndex) = cpuBufferIdd.getRawData(bindingTablePointerIndex);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setGenerateLocalId(bool generateLocalIdsByGpu, uint32_t walkOrder, uint32_t localIdDimensions) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    constexpr uint32_t localIdGenerationIndex = 4;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (!!cpuBufferWalker->getGenerateLocalId() == generateLocalIdsByGpu &&
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
void MutableComputeWalkerHw<GfxFamily>::setNumberWorkGroups(std::array<uint32_t, 3> numberWorkGroups) {
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
void MutableComputeWalkerHw<GfxFamily>::setWorkGroupSize(std::array<uint32_t, 3> workgroupSize) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    constexpr uint32_t workGroupSizeIndex = 6;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->setLocalXMaximum(workgroupSize[0] - 1);
    cpuBufferWalker->setLocalYMaximum(workgroupSize[1] - 1);
    cpuBufferWalker->setLocalZMaximum(workgroupSize[2] - 1);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getRawData(workGroupSizeIndex) = cpuBufferWalker->getRawData(workGroupSizeIndex);
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
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
    walkerCmd->getPostSync().setDestinationAddress(postSyncAddress);

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->getPostSync().setDestinationAddress(postSyncAddress);
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

    return offsetof(WalkerType, TheStructure.Common.InlineData);
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
    using PostSyncData = typename GfxFamily::POSTSYNC_DATA;
    auto sourceWalkerCmdHostBuffer = reinterpret_cast<WalkerType *>(static_cast<MutableComputeWalkerHw<GfxFamily> *>(sourceWalker)->cpuBuffer);
    auto walkerCmdHostBuffer = reinterpret_cast<WalkerType *>(this->cpuBuffer);

    walkerCmdHostBuffer->setIndirectDataStartAddress(sourceWalkerCmdHostBuffer->getIndirectDataStartAddress());

    memcpy_s(&walkerCmdHostBuffer->getPostSync(), sizeof(PostSyncData), &sourceWalkerCmdHostBuffer->getPostSync(), sizeof(PostSyncData));
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::updateWalkerScratchPatchAddress(GpuAddress scratchPatchAddress) {
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::saveCpuBufferIntoGpuBuffer(bool useDispatchPart) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto walkerCmdHostBuffer = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    if (useDispatchPart) {
        constexpr size_t dispatchPartSize = offsetof(WalkerType, TheStructure.Common.PostSync);
        memcpy_s(this->walker, dispatchPartSize, this->cpuBuffer, dispatchPartSize);
    } else {
        *walkerCmd = *walkerCmdHostBuffer;
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
            .localRegionSize = NEO::localRegionSizeParamNotSet,
            .maxFrontEndThreads = device.getDeviceInfo().maxFrontEndThreads,
            .requiredSystemFence = false,
            .hasSample = false};

        NEO::EncodeDispatchKernel<GfxFamily>::encodeComputeDispatchAllWalker(*cpuBufferWalker, &idd, device.getRootDeviceEnvironment(), encodeWalkerArgs);
    }

    if (args.isSlmKernel && (args.updateGroupSize || args.updateSlm)) {
        NEO::EncodeDispatchKernel<GfxFamily>::setupPreferredSlmSize(&idd,
                                                                    device.getRootDeviceEnvironment(),
                                                                    args.threadsPerThreadGroup,
                                                                    args.slmTotalSize,
                                                                    static_cast<NEO::SlmPolicy>(args.slmPolicy));
    }

    if (args.updateGroupCount || args.updateGroupSize) {
        if (args.partitionCount > 1) {
            this->updateImplicitScalingData<WalkerType>(device,
                                                        args.partitionCount, args.totalWorkGroupSize, args.threadGroupCount, args.maxWgCountPerTile,
                                                        args.requiredPartitionDim, args.isRequiredDispatchWorkGroupOrder, args.cooperativeKernel);
        }
    }

    if (!this->stageCommitMode) {
        if (args.partitionCount == 1) {
            constexpr uint32_t computeDispatchAllWalkerEnableIndex = 4;

            auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
            walkerCmd->getInterfaceDescriptor() = idd;

            walkerCmd->getRawData(computeDispatchAllWalkerEnableIndex) = cpuBufferWalker->getRawData(computeDispatchAllWalkerEnableIndex);
        } else {
            this->saveCpuBufferIntoGpuBuffer(true);
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
