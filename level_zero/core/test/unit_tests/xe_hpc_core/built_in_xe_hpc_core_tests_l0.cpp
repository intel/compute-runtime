/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_recompile_built_ins.h"

namespace L0 {
namespace ult {

using XeHpcCoreBuiltInTestL0 = Test<NEO::DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(BuiltInSharedTest, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenBuiltinFromDefaultRevisionIsTaken, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(BuiltInTestL0, givenDeviceWithUnregisteredBinaryBuiltinWhenGettingBuiltinKernelThenTakeBinaryBuiltinFromDefaultRevision, IGFX_XE_HPC_CORE);

HWTEST2_F(XeHpcCoreBuiltInTestL0, givenDeviceWithUnregisteredBinaryBuiltinWhenGettingBuiltinKernelThenFallbackToIntermediate, IsXeHpcCore) {
    pDevice->incRefInternal();
    L0::ult::MockDeviceForRebuildBuilins deviceL0(pDevice);
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    L0::BuiltinFunctionsLibImpl builtinFunctionsLib{&deviceL0, pDevice->getBuiltIns()};
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(L0::Builtin::COUNT); builtId++) {
        deviceL0.formatForModule = {};
        ASSERT_NE(nullptr, builtinFunctionsLib.getFunction(static_cast<L0::Builtin>(builtId)));
        EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, deviceL0.formatForModule);
    }
}

HWTEST2_F(XeHpcCoreBuiltInTestL0, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceForNotRegisteredRevisionThenResourceSizeIsZero, IsXeHpcCore) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImage3dToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Binary, *pDevice).size());
}
} // namespace ult
} // namespace L0