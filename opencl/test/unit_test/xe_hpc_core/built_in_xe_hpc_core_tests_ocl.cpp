/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;
using XeHpcCoreBuiltInTests = Test<DeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreBuiltInTests, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenBinaryBuiltinIsNotAvailable) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    const std::array<uint32_t, 11> builtinTypes{EBuiltInOps::CopyBufferToBuffer,
                                                EBuiltInOps::CopyBufferRect,
                                                EBuiltInOps::FillBuffer,
                                                EBuiltInOps::CopyBufferToImage3d,
                                                EBuiltInOps::CopyImage3dToBuffer,
                                                EBuiltInOps::CopyImageToImage1d,
                                                EBuiltInOps::CopyImageToImage2d,
                                                EBuiltInOps::CopyImageToImage3d,
                                                EBuiltInOps::FillImage1d,
                                                EBuiltInOps::FillImage2d,
                                                EBuiltInOps::FillImage3d};

    for (auto &builtinType : builtinTypes) {
        EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(builtinType, BuiltinCode::ECodeType::Binary, *pDevice).size());
    }
}
