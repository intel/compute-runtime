/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

using DebugApiTest = Test<DebugApiFixture>;
HWTEST2_F(DebugApiTest, givenDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsCorrectFlagForXeHPG, IsARL) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

} // namespace ult
} // namespace L0
