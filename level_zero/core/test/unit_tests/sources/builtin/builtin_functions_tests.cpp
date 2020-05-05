/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/unit_test/mocks/mock_compiler_interface.h"

#include "test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
struct BuiltinFunctionsLibImpl::BuiltinData {
    ~BuiltinData() {
        func.reset();
        module.reset();
    }

    std::unique_ptr<Module> module;
    std::unique_ptr<Kernel> func;
};
namespace ult {
class TestBuiltinFunctionsLibImpl : public DeviceFixture, public testing::Test {
  public:
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
    };

    void SetUp() override {
        DeviceFixture::SetUp();
        mockBuiltinFunctionsLibImpl.reset(new MockBuiltinFunctionsLibImpl(device, neoDevice->getBuiltIns()));
    }
    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<MockBuiltinFunctionsLibImpl> mockBuiltinFunctionsLibImpl;
};

HWTEST_F(TestBuiltinFunctionsLibImpl, givenInitImageFunctionWhenImageBultinsTableConstainNullptrsAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }
    if (device->getHwInfo().capabilityTable.supportsImages) {
        mockBuiltinFunctionsLibImpl->initImageFunctions();
        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
            EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        }
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenInitFunctionWhenBultinsTableConstainNullptrsThenBuiltinsFunctionsAreLoaded) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }
    mockBuiltinFunctionsLibImpl->initFunctions();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterface());
    std::unique_ptr<L0::Device> testDvice(Device::create(device->getDriverHandle(), neoDevice));

    if (device->getHwInfo().capabilityTable.supportsImages) {
        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_NE(nullptr, testDvice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        }
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterface());
    std::unique_ptr<L0::Device> testDvice(Device::create(device->getDriverHandle(), neoDevice));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, testDvice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}
} // namespace ult
} // namespace L0
