/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWTEST_F(CommandEncodeStatesTest, encodeCopySamplerState) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    uint32_t numSamplers = 1;
    SAMPLER_STATE samplerState;

    auto dsh = cmdContainer->getIndirectHeap(HeapType::DYNAMIC_STATE);
    auto usedBefore = dsh->getUsed();
    auto samplerStateOffset = EncodeStates<FamilyType>::copySamplerState(dsh, 0, numSamplers, 0, &samplerState);

    auto pSmplr = reinterpret_cast<SAMPLER_STATE *>(ptrOffset(dsh->getCpuBase(), samplerStateOffset));
    EXPECT_EQ(pSmplr->getIndirectStatePointer(), usedBefore);
}

HWTEST_F(CommandEncodeStatesTest, givenCreatedSurfaceStateBufferWhenAllocationProvidedThenUseAllocationAsInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);
    GraphicsAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull);
    EncodeSurfaceState<FamilyType>::encodeBuffer(stateBuffer, reinterpret_cast<void *>(gpuAddr), allocSize, 1,
                                                 RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(gpuAddr, state->getSurfaceBaseAddress());

    alignedFree(stateBuffer);
}

HWTEST_F(CommandEncodeStatesTest, givenCreatedSurfaceStateBufferWhenAllocationNotProvidedThenStateTypeIsNull) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;

    uint64_t gpuAddr = 0;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);

    EncodeSurfaceState<FamilyType>::encodeBuffer(stateBuffer, reinterpret_cast<void *>(gpuAddr), allocSize, 1,
                                                 RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, state->getSurfaceType());

    alignedFree(stateBuffer);
}

HWTEST_F(CommandEncodeStatesTest, givenCreatedSurfaceStateBufferWhenGpuCoherencyProvidedThenCoherencyGpuIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;

    uint64_t gpuAddr = 0;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);

    EncodeSurfaceState<FamilyType>::encodeBuffer(stateBuffer, reinterpret_cast<void *>(gpuAddr), allocSize, 1,
                                                 RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);

    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, state->getCoherencyType());

    alignedFree(stateBuffer);
}

HWTEST_F(CommandEncodeStatesTest, givenCommandContainerWithDirtyHeapsWhenSetStateBaseAddressCalledThenStateBaseAddressAreNotSet) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    cmdContainer->dirtyHeaps = 0;
    auto baseAddres = cmdContainer->getCommandStream()->getCpuBase();

    cmdContainer->setHeapDirty(NEO::HeapType::DYNAMIC_STATE);
    cmdContainer->setHeapDirty(NEO::HeapType::INDIRECT_OBJECT);
    cmdContainer->setHeapDirty(NEO::HeapType::SURFACE_STATE);

    EncodeStateBaseAddress<FamilyType>::encode(*cmdContainer.get());

    auto dsh = cmdContainer->getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ioh = cmdContainer->getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ssh = cmdContainer->getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    auto pCmd = static_cast<STATE_BASE_ADDRESS *>(baseAddres);

    EXPECT_EQ(dsh->getHeapGpuBase(), pCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(ioh->getHeapGpuBase(), pCmd->getIndirectObjectBaseAddress());
    EXPECT_EQ(ssh->getHeapGpuBase(), pCmd->getSurfaceStateBaseAddress());
}

HWTEST_F(CommandEncodeStatesTest, givenCommandContainerWhenSetStateBaseAddressCalledThenStateBaseAddressIsSetCorrectly) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    cmdContainer->dirtyHeaps = 0;

    EncodeStateBaseAddress<FamilyType>::encode(*cmdContainer.get());

    auto dsh = cmdContainer->getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ssh = cmdContainer->getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);

    EXPECT_NE(dsh->getHeapGpuBase(), cmd->getDynamicStateBaseAddress());
    EXPECT_NE(ssh->getHeapGpuBase(), cmd->getSurfaceStateBaseAddress());
}

HWTEST_F(CommandEncodeStatesTest, givenAnAlignedDstPtrThenNoAlignmentNorOffsetNeeded) {
    uintptr_t ptr = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment() << 1;
    size_t offset = 0;
    NEO::EncodeSurfaceState<FamilyType>::getSshAlignedPointer(ptr, offset);
    EXPECT_TRUE((ptr & (NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment() - 1)) == 0x0u);
    EXPECT_EQ(0u, offset);
}

HWTEST_F(CommandEncodeStatesTest, givenAnUnalignedDstPtrThenCorrectAlignedPtrAndOffsetAreCalculated) {
    uintptr_t ptr = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment() >> 1;
    size_t offset = 0;
    NEO::EncodeSurfaceState<FamilyType>::getSshAlignedPointer(ptr, offset);
    EXPECT_TRUE((ptr & (NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment() - 1)) == 0x0u);
    EXPECT_NE(0u, offset);
}