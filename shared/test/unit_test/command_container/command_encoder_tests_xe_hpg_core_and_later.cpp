/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;
using CommandEncoderXeHpgCorePlusTests = Test<DeviceFixture>;

HWTEST2_F(CommandEncoderXeHpgCorePlusTests, givenSpecifiedL1CacheControlWhenAppendingRssThenProgramProvidedL1CachePolicy, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    auto l1CacheControl = FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP;
    DebugManager.flags.OverrideL1CacheControlInSurfaceState.set(l1CacheControl);

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    args.numAvailableDevices = pDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(l1CacheControl), rssCmd.getL1CachePolicyL1CacheControl());

    l1CacheControl = FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB;
    DebugManager.flags.OverrideL1CacheControlInSurfaceState.set(l1CacheControl);
    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(l1CacheControl), rssCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceAllResourcesUncached.set(true);
    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_UC), rssCmd.getL1CachePolicyL1CacheControl());

    memoryManager->freeGraphicsMemory(allocation);
}
