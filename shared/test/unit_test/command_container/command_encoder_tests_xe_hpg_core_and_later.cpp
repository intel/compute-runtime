/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "encode_surface_state_args.h"

using namespace NEO;
using CommandEncoderXeHpgCorePlusTests = Test<DeviceFixture>;

HWTEST2_F(CommandEncoderXeHpgCorePlusTests, givenSpecifiedL1CacheControlWhenAppendingRssThenProgramProvidedL1CachePolicy, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::buffer, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    auto l1CacheControl = FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;
    debugManager.flags.OverrideL1CacheControlInSurfaceState.set(l1CacheControl);

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = pDevice->getGmmHelper()->getL3EnabledMOCS();
    args.numAvailableDevices = pDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(l1CacheControl), rssCmd.getL1CacheControlCachePolicy());

    l1CacheControl = FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB;
    debugManager.flags.OverrideL1CacheControlInSurfaceState.set(l1CacheControl);
    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(l1CacheControl), rssCmd.getL1CacheControlCachePolicy());

    debugManager.flags.ForceAllResourcesUncached.set(true);
    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(static_cast<uint32_t>(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_UC), rssCmd.getL1CacheControlCachePolicy());

    memoryManager->freeGraphicsMemory(allocation);
}
