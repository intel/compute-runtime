/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {
namespace ult {

using MutableHwCommandTestXeCore = Test<MutableHwCommandFixture>;

HWTEST2_F(MutableHwCommandTestXeCore, givenMutableComputeWalkerWhenSettingKernelStartAddressThenCorrectValueSet, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint64_t kernelStartAddress = 0xAABB0000;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setKernelStartAddress(kernelStartAddress);
    // in stage commit mode, the kernel start address is not set in the command buffer at the time of the call, only when committed
    EXPECT_EQ(kernelStartAddress, cpuBuffer->getInterfaceDescriptor().getKernelStartPointer());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getKernelStartPointer());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setKernelStartAddress(kernelStartAddress);
    EXPECT_EQ(kernelStartAddress, cpuBuffer->getInterfaceDescriptor().getKernelStartPointer());
    EXPECT_EQ(kernelStartAddress, walkerCmd->getInterfaceDescriptor().getKernelStartPointer());

    // if cpu buffer is the same as argument, further update is stopped, verify it by cleaning gpu buffer
    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->setKernelStartAddress(kernelStartAddress);
    EXPECT_EQ(kernelStartAddress, cpuBuffer->getInterfaceDescriptor().getKernelStartPointer());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getKernelStartPointer());
}

HWTEST2_F(MutableHwCommandTestXeCore, givenMutableComputeWalkerWhenSettingIndirectDataAddressThenCorrectValueSet, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint64_t indirectAddress = 0xAABB0000;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setIndirectDataStartAddress(indirectAddress);

    EXPECT_EQ(indirectAddress, cpuBuffer->getIndirectDataStartAddress());
    EXPECT_EQ(0u, walkerCmd->getIndirectDataStartAddress());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setIndirectDataStartAddress(indirectAddress);
    EXPECT_EQ(indirectAddress, cpuBuffer->getIndirectDataStartAddress());
    EXPECT_EQ(indirectAddress, walkerCmd->getIndirectDataStartAddress());
}

HWTEST2_F(MutableHwCommandTestXeCore, givenMutableComputeWalkerWhenSettingIndirectDataSizeThenCorrectValueSet, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    size_t indirectSize = 0x100;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setIndirectDataSize(indirectSize);

    EXPECT_EQ(indirectSize, cpuBuffer->getIndirectDataLength());
    EXPECT_EQ(0u, walkerCmd->getIndirectDataLength());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setIndirectDataSize(indirectSize);
    EXPECT_EQ(indirectSize, cpuBuffer->getIndirectDataLength());
    EXPECT_EQ(indirectSize, walkerCmd->getIndirectDataLength());
}

HWTEST2_F(MutableHwCommandTestXeCore, givenMutableComputeWalkerWhenSettingBindingTablePointerThenCorrectValueSet, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint32_t bindingTableAddress = 0x40;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setBindingTablePointer(bindingTableAddress);

    EXPECT_EQ(bindingTableAddress, cpuBuffer->getInterfaceDescriptor().getBindingTablePointer());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getBindingTablePointer());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setBindingTablePointer(bindingTableAddress);
    EXPECT_EQ(bindingTableAddress, cpuBuffer->getInterfaceDescriptor().getBindingTablePointer());
    EXPECT_EQ(bindingTableAddress, walkerCmd->getInterfaceDescriptor().getBindingTablePointer());
}

HWTEST2_F(MutableHwCommandTestXeCore, givenTwoMutableComputeWalkersWhenCopyingDataToHostPointerThenDataIsCopied, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    alignas(8) uint8_t srcWalkerGpuBuffer[sizeof(WalkerType)];
    void *srcWalkerCpuBuffer = L0::MCL::MutableComputeWalkerHw<FamilyType>::createCommandBuffer();
    constexpr uint8_t srcWalkerIndirectOffset = 0x0;
    constexpr uint8_t srcWalkerScratchOffset = 0x8;

    L0::MCL::MutableComputeWalkerHw<FamilyType> srcMutableWalker(srcWalkerGpuBuffer, srcWalkerIndirectOffset, srcWalkerScratchOffset, srcWalkerCpuBuffer, false);
    memcpy(srcWalkerCpuBuffer, &walkerTemplate, sizeof(WalkerType));
    auto srcWalkerCpuCmd = reinterpret_cast<WalkerType *>(srcWalkerCpuBuffer);

    uint64_t postSyncAddress = 0xCCDD0000;
    uint32_t indirectAddress = 0xAABB0000;

    srcWalkerCpuCmd->setIndirectDataStartAddress(indirectAddress);
    srcWalkerCpuCmd->getPostSync().setDestinationAddress(postSyncAddress);

    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto walkerCpuCmd = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->copyWalkerDataToHostBuffer(&srcMutableWalker);

    EXPECT_EQ(indirectAddress, walkerCpuCmd->getIndirectDataStartAddress());
    EXPECT_EQ(postSyncAddress, walkerCpuCmd->getPostSync().getDestinationAddress());
}

HWTEST2_F(MutableHwCommandTestXeCore, givenMutableComputeWalkerWhenSettingScratchAddressThenNoActionIsTaken, IsWithinXeCoreAndXe3Core) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    size_t scratchPatchAddress = 0xF000;

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    mutableWalker->updateWalkerScratchPatchAddress(scratchPatchAddress);

    EXPECT_EQ(0, memcmp(this->cmdBufferCpuPtr, &walkerTemplate, this->walkerCmdSize));
    EXPECT_EQ(0, memcmp(this->cmdBufferGpuPtr, &walkerTemplate, this->walkerCmdSize));
}

} // namespace ult
} // namespace L0
