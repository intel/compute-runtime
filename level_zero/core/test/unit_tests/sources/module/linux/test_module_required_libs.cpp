/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using ModuleRequiredLibsTests = Test<DeviceFixture>;

TEST_F(ModuleRequiredLibsTests, givenRequirementsInProgramInfoWhenLinkingAModuleThenRequiredLibsAreTakenFromDeviceAndDynamicallyLinked) {
    auto mockDevice = MockDevice(neoDevice);
    mockDevice.getHwInfoResultPtr = defaultHwInfo.get();
    mockDevice.getGfxCoreHelperResultPtr = execEnv->rootDeviceEnvironments[rootDeviceIndex]->gfxCoreHelper.get();

    auto module = std::make_unique<WhiteBox<::L0::Module>>(&mockDevice, nullptr, ModuleType::user);
    auto libModule = std::make_unique<MockModule>(&mockDevice, nullptr, ModuleType::user);
    mockDevice.getRequiredLibModuleCalled = 0;
    mockDevice.getRequiredLibModuleResult = libModule.get();

    module->translationUnit->programInfo.requiredLibs.push_back("libFoo");
    module->translationUnit->programInfo.requiredLibs.push_back("libBar");
    module->performDynamicLinkCallBase = false;
    module->performDynamicLinkCalled = 0;

    bool ret = module->linkInternalRequiredLibsModule();
    EXPECT_TRUE(ret);
    EXPECT_EQ(2U, mockDevice.getRequiredLibModuleCalled);
    EXPECT_EQ(1, module->performDynamicLinkCalled);
    EXPECT_EQ(3U, module->performDynamicLinkRecordedPhModules.size());
    EXPECT_EQ(module->toHandle(), module->performDynamicLinkRecordedPhModules[0]);
    EXPECT_EQ(libModule->toHandle(), module->performDynamicLinkRecordedPhModules[1]);
    // returned module pointer is the same to avoid complicating mock's implementation
    EXPECT_EQ(libModule->toHandle(), module->performDynamicLinkRecordedPhModules[2]);
}

TEST_F(ModuleRequiredLibsTests, givenRequirementsInProgramInfoWhenDynamicLinkingFailsThenFunctionReturnsFalse) {
    auto mockDevice = MockDevice(neoDevice);
    mockDevice.getHwInfoResultPtr = defaultHwInfo.get();
    mockDevice.getGfxCoreHelperResultPtr = execEnv->rootDeviceEnvironments[rootDeviceIndex]->gfxCoreHelper.get();

    auto module = std::make_unique<WhiteBox<::L0::Module>>(&mockDevice, nullptr, ModuleType::user);
    auto libModule = std::make_unique<MockModule>(&mockDevice, nullptr, ModuleType::user);
    mockDevice.getRequiredLibModuleCalled = 0;
    mockDevice.getRequiredLibModuleResult = libModule.get();

    module->translationUnit->programInfo.requiredLibs.push_back("libFoo");
    module->performDynamicLinkCallBase = false;
    module->performDynamicLinkCalled = 0;
    module->performDynamicLinkResult = ZE_RESULT_ERROR_MODULE_LINK_FAILURE;

    bool ret = module->linkInternalRequiredLibsModule();
    EXPECT_FALSE(ret);
    EXPECT_EQ(1, module->performDynamicLinkCalled);
    EXPECT_EQ(1U, mockDevice.getRequiredLibModuleCalled);
    EXPECT_FALSE(module->isFullyLinked);
}

TEST_F(ModuleRequiredLibsTests, givenNoRequiredLibsInProgramInfoWhenLinkingFunctionCalledThenItReturnsEarly) {
    auto mockDevice = MockDevice(neoDevice);
    mockDevice.getHwInfoResultPtr = defaultHwInfo.get();
    mockDevice.getGfxCoreHelperResultPtr = execEnv->rootDeviceEnvironments[rootDeviceIndex]->gfxCoreHelper.get();

    auto module = std::make_unique<WhiteBox<::L0::Module>>(&mockDevice, nullptr, ModuleType::user);
    mockDevice.getRequiredLibModuleCalled = 0;
    mockDevice.getRequiredLibModuleResult = nullptr;

    module->translationUnit->programInfo.requiredLibs.clear();
    module->performDynamicLinkCallBase = false;
    module->performDynamicLinkCalled = 0;

    bool ret = module->linkInternalRequiredLibsModule();
    EXPECT_TRUE(ret);
    EXPECT_EQ(0U, mockDevice.getRequiredLibModuleCalled);
    EXPECT_EQ(0, module->performDynamicLinkCalled);
}

TEST_F(ModuleRequiredLibsTests, givenRequiredLibsRegistryWhenLibModuleCreatedThenItIsReusableForTheSameDeviceInstance) {
    auto mockDevice = MockDeviceImp(neoDevice);

    auto module = std::make_unique<WhiteBox<::L0::Module>>(&mockDevice, nullptr, ModuleType::user);
    auto libModule = std::make_unique<MockModule>(&mockDevice, nullptr, ModuleType::user);
    mockDevice.createRequiredLibModuleCalled = 0;
    mockDevice.createRequiredLibModuleResult = libModule.get();

    auto pModule1 = mockDevice.getRequiredLibModule("libFoo", nullptr);
    EXPECT_EQ(libModule.get(), pModule1);
    auto pModule2 = mockDevice.getRequiredLibModule("libFoo", nullptr);
    EXPECT_EQ(libModule.get(), pModule2);
    EXPECT_EQ(1U, mockDevice.createRequiredLibModuleCalled);
}

} // namespace ult
} // namespace L0
