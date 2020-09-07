/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/mock_compiler_interface.h"

#include "test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
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

HWTEST_F(TestBuiltinFunctionsLibImpl, givenInitImageFunctionWhenImageBultinsTableContainNullptrsAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
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

HWTEST_F(TestBuiltinFunctionsLibImpl, givenInitFunctionWhenBultinsTableContainNullptrsThenBuiltinsFunctionsAreLoaded) {
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
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, std::numeric_limits<uint32_t>::max(), false));

    if (device->getHwInfo().capabilityTable.supportsImages) {
        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        }
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterface());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, std::numeric_limits<uint32_t>::max(), false));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenIntermediateCodeForBuiltinsIsRequested) {
    struct MockDeviceForRebuildBuilins : public Mock<DeviceImp> {
        struct MockModuleForRebuildBuiltins : public ModuleImp {
            MockModuleForRebuildBuiltins(Device *device) : ModuleImp(device, nullptr) {}

            ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                     ze_kernel_handle_t *phFunction) override {
                *phFunction = nullptr;
                return ZE_RESULT_SUCCESS;
            }
        };

        MockDeviceForRebuildBuilins(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            driverHandle = device->getDriverHandle();
            builtins = BuiltinFunctionsLib::create(this, neoDevice->getBuiltIns());
        }

        ze_result_t createModule(const ze_module_desc_t *desc,
                                 ze_module_handle_t *module,
                                 ze_module_build_log_handle_t *buildLog) override {
            EXPECT_EQ(desc->format, ZE_MODULE_FORMAT_IL_SPIRV);
            EXPECT_GT(desc->inputSize, 0u);
            EXPECT_NE(desc->pInputModule, nullptr);
            wasCreatedModuleCalled = true;

            *module = new MockModuleForRebuildBuiltins(this);

            return ZE_RESULT_SUCCESS;
        }

        bool wasCreatedModuleCalled = false;
    };

    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);
    MockDeviceForRebuildBuilins testDevice(device);

    testDevice.getBuiltinFunctionsLib()->initFunctions();

    EXPECT_TRUE(testDevice.wasCreatedModuleCalled);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenNotToRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenNativeCodeForBuiltinsIsRequested) {
    struct MockDeviceForRebuildBuilins : public Mock<DeviceImp> {
        MockDeviceForRebuildBuilins(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            driverHandle = device->getDriverHandle();
            builtins = BuiltinFunctionsLib::create(this, neoDevice->getBuiltIns());
        }

        ze_result_t createModule(const ze_module_desc_t *desc,
                                 ze_module_handle_t *module,
                                 ze_module_build_log_handle_t *buildLog) override {
            EXPECT_EQ(desc->format, ZE_MODULE_FORMAT_NATIVE);
            EXPECT_GT(desc->inputSize, 0u);
            EXPECT_NE(desc->pInputModule, nullptr);
            wasCreatedModuleCalled = true;

            return DeviceImp::createModule(desc, module, buildLog);
        }

        bool wasCreatedModuleCalled = false;
    };

    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(false);
    MockDeviceForRebuildBuilins testDevice(device);

    testDevice.getBuiltinFunctionsLib()->initFunctions();

    EXPECT_TRUE(testDevice.wasCreatedModuleCalled);
}

} // namespace ult
} // namespace L0
