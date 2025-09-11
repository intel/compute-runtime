/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/multi_tile_fixture.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
MultiTileCommandListAppendLaunchKernelFixture::MultiTileCommandListAppendLaunchKernelFixture() : backup({&NEO::ImplicitScaling::apiSupport, true}) {}
MultiTileImmediateCommandListAppendLaunchKernelFixture::MultiTileImmediateCommandListAppendLaunchKernelFixture() : backupApiSupport({&NEO::ImplicitScaling::apiSupport, true}) {}

void MultiTileCommandListAppendLaunchKernelFixture::setUp() {
    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::numSubDevices = 4u;

    MultiDeviceModuleFixture::setUp();
    createModuleFromMockBinary(0u);
    createKernel(0u);

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList = CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

void MultiTileCommandListAppendLaunchKernelFixture::tearDown() {
    commandList->destroy();
    contextImp->destroy();

    MultiDeviceModuleFixture::tearDown();
}

void MultiTileImmediateCommandListAppendLaunchKernelFixture::setUp() {
    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::numSubDevices = 2u;

    MultiDeviceModuleFixture::setUp();
    createModuleFromMockBinary(0u);
    createKernel(0u);

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));
}

void MultiTileImmediateCommandListAppendLaunchKernelFixture::tearDown() {
    contextImp->destroy();

    MultiDeviceModuleFixture::tearDown();
}

} // namespace ult
} // namespace L0
