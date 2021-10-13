/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

using namespace NEO;

using BuiltInTestL0 = Test<DeviceFixture>;

TEST_F(BuiltInTestL0, GivenBuiltinTypeIntermediateWhenGettingBuiltinResourceForNotRegisteredRevisionThenResourceSizeIsNonZero) {
    class MockBuiltinsLib : BuiltinsLib {
      public:
        BuiltinResourceT getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
            return BuiltinsLib::getBuiltinResource(builtin, requestedCodeType, device);
        }
    };
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToImage3d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImage3dToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage1d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage2d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage3d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage1d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage2d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage3d, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Intermediate, *pDevice).size());
}