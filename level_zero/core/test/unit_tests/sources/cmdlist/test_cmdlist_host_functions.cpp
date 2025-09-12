/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include <level_zero/driver_experimental/zex_cmdlist.h>

namespace L0 {
namespace ult {

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenRegularCommandListWhenZexCommandListAppendHostFunctionIsCalledThenUnsupportedFeatureIsReturned) {

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    auto result =
        zexCommandListAppendHostFunction(commandList->toHandle(), nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST_F(HostFunctionTests, givenImmediateCommandListWhenZexCommandListAppendHostFunctionIsCalledThenUnsupportedFeatureIsReturned) {

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    auto result =
        zexCommandListAppendHostFunction(commandList->toHandle(), nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace L0
