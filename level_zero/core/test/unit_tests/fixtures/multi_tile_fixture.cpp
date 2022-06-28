/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/multi_tile_fixture.h"

#include "level_zero/core/source/context/context_imp.h"

namespace L0 {
namespace ult {

void MultiTileCommandListAppendLaunchFunctionFixture::SetUp() {
    DebugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::numSubDevices = 4u;

    MultiDeviceModuleFixture::SetUp();
    createModuleFromBinary(0u);
    createKernel(0u);

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

void MultiTileCommandListAppendLaunchFunctionFixture::TearDown() {
    commandList->destroy();
    contextImp->destroy();

    MultiDeviceModuleFixture::TearDown();
}

void MultiTileImmediateCommandListAppendLaunchFunctionFixture::SetUp() {
    DebugManager.flags.EnableImplicitScaling.set(1);

    MultiDeviceFixture::numRootDevices = 1u;
    MultiDeviceFixture::numSubDevices = 2u;

    MultiDeviceModuleFixture::SetUp();
    createModuleFromBinary(0u);
    createKernel(0u);

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));
}

void MultiTileImmediateCommandListAppendLaunchFunctionFixture::TearDown() {
    contextImp->destroy();

    MultiDeviceModuleFixture::TearDown();
}

} // namespace ult
} // namespace L0
