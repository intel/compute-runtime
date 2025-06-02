/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
const uint32_t UnitTestHelper<GfxFamily>::smallestTestableSimdSize = 16;

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDebugModeRegisterOffset() {
    return 0x20d8;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDebugModeRegisterValue() {
    return (1u << 5) | (1u << 21);
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getTdCtlRegisterValue() {
    return (1u << 7) | (1u << 4) | (1u << 2) | (1u << 0);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isL3ConfigProgrammable() {
    return false;
};

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex) {
    if (kernelDescriptor == nullptr) {
        if (sizeBeforeEnqueue == sizeAfterEnqueue) {
            return true;
        }
        return false;
    }

    auto samplerCount = kernelDescriptor->payloadMappings.samplerTable.numSamplers;
    if (samplerCount > 0) {
        if (sizeBeforeEnqueue != sizeAfterEnqueue) {
            return true;
        }
        return false;
    } else {
        if (sizeBeforeEnqueue == sizeAfterEnqueue) {
            return true;
        }
        return false;
    }
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isTimestampPacketWriteSupported() {
    return true;
};

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isExpectMemoryNotEqualSupported() {
    return true;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDefaultSshUsage() {
    return (32 * 2 * 64);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait) {
    return (semaphoreWait.getSemaphoreDataDword() == EncodeSemaphore<GfxFamily>::invalidHardwareTag);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress) {
    return 0llu == retrievedGshAddress;
}

template <typename GfxFamily>
auto UnitTestHelper<GfxFamily>::getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType) {
    return GfxFamily::RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT;
}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::adjustKernelDescriptorForImplicitArgs(KernelDescriptor &kernelDescriptor) {
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
}

template <typename GfxFamily>
std::vector<bool> UnitTestHelper<GfxFamily>::getProgrammedLargeGrfValues(CommandStreamReceiver &csr, LinearStream &linearStream) {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;

    std::vector<bool> largeGrfValues;
    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(csr, linearStream);
    auto commands = hwParser.getCommandsList<STATE_COMPUTE_MODE>();
    for (auto &cmd : commands) {
        largeGrfValues.push_back(reinterpret_cast<STATE_COMPUTE_MODE *>(cmd)->getLargeGrfMode());
    }
    return largeGrfValues;
}

template <typename GfxFamily>
std::vector<bool> UnitTestHelper<GfxFamily>::getProgrammedLargeGrfValues(LinearStream &linearStream) {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;

    std::vector<bool> largeGrfValues;
    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(linearStream);
    auto commands = hwParser.getCommandsList<STATE_COMPUTE_MODE>();
    for (auto &cmd : commands) {
        largeGrfValues.push_back(reinterpret_cast<STATE_COMPUTE_MODE *>(cmd)->getLargeGrfMode());
    }
    return largeGrfValues;
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::getWorkloadPartitionForStoreRegisterMemCmd(typename GfxFamily::MI_STORE_REGISTER_MEM &storeRegisterMem) {
    return storeRegisterMem.getWorkloadPartitionIdOffsetEnable();
}

template <typename GfxFamily>
size_t UnitTestHelper<GfxFamily>::getAdditionalDshSize(uint32_t iddCount) {
    return 0;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::verifyDummyBlitWa(const RootDeviceEnvironment *rootDeviceEnvironment, GenCmdList::iterator &cmdIterator) {
    const auto releaseHelper = rootDeviceEnvironment->getReleaseHelper();
    if (releaseHelper->isDummyBlitWaRequired()) {
        using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
        auto dummyBltCmd = genCmdCast<XY_COLOR_BLT *>(*(cmdIterator++));
        EXPECT_NE(nullptr, dummyBltCmd);

        auto expectedDestinationBaseAddress = rootDeviceEnvironment->getDummyAllocation()->getGpuAddress();

        EXPECT_EQ(expectedDestinationBaseAddress, dummyBltCmd->getDestinationBaseAddress());
        EXPECT_EQ(XY_COLOR_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR, dummyBltCmd->getColorDepth());
        EXPECT_EQ(1u, dummyBltCmd->getDestinationX2CoordinateRight());
        EXPECT_EQ(4u, dummyBltCmd->getDestinationY2CoordinateBottom());
        EXPECT_EQ(static_cast<uint32_t>(MemoryConstants::pageSize), dummyBltCmd->getDestinationPitch());
        EXPECT_EQ(XY_COLOR_BLT::DESTINATION_SURFACE_TYPE::DESTINATION_SURFACE_TYPE_SURFTYPE_2D, dummyBltCmd->getDestinationSurfaceType());
    }
}

template <typename GfxFamily>
GenCmdList::iterator UnitTestHelper<GfxFamily>::findWalkerTypeCmd(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return find<typename GfxFamily::COMPUTE_WALKER *>(begin, end);
}

template <typename GfxFamily>
std::vector<GenCmdList::iterator> UnitTestHelper<GfxFamily>::findAllWalkerTypeCmds(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return findAll<typename GfxFamily::COMPUTE_WALKER *>(begin, end);
}

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(bool isHeaplessEnabled, WalkerPartition::WalkerPartitionArgs &testArgs) {
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;

    return WalkerPartition::estimateSpaceRequiredInCommandBuffer<GfxFamily, DefaultWalkerType>(testArgs);
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::getSpaceAndInitWalkerCmd(LinearStream &stream, bool heapless) {
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    *stream.getSpaceForCmd<COMPUTE_WALKER>() = GfxFamily::template getInitGpuWalker<COMPUTE_WALKER>();
}

template <typename GfxFamily>
void *UnitTestHelper<GfxFamily>::getInitWalkerCmd(bool heapless) {
    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
    return new COMPUTE_WALKER;
}

template <typename GfxFamily>
template <typename WalkerType>
uint64_t UnitTestHelper<GfxFamily>::getWalkerActivePostSyncAddress(WalkerType *walkerCmd) {
    return walkerCmd->getPostSync().getDestinationAddress();
}

} // namespace NEO
