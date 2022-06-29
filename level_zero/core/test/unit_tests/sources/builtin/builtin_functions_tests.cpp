/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_compiler_interface_spirv.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
template <bool useImagesBuiltins, bool isStateless>
class BuiltinFunctionsLibFixture : public DeviceFixture {
  public:
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {
        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
        std::unique_ptr<BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
            ze_result_t res;
            std::unique_ptr<Module> module;
            ze_module_handle_t moduleHandle;
            ze_module_desc_t moduleDesc = {};
            moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
            moduleDesc.pInputModule = nullptr;
            moduleDesc.inputSize = 0u;
            res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::Builtin);
            UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

            module.reset(Module::fromHandle(moduleHandle));

            std::unique_ptr<Kernel> kernel;
            ze_kernel_handle_t kernelHandle;
            ze_kernel_desc_t kernelDesc = {};
            kernelDesc.pKernelName = builtInName;
            res = module->createKernel(&kernelDesc, &kernelHandle);
            DEBUG_BREAK_IF(res != ZE_RESULT_SUCCESS);

            kernel.reset(Kernel::fromHandle(kernelHandle));

            return std::unique_ptr<BuiltinData>(new MockBuiltinData{std::move(module), std::move(kernel)});
        }
    };
    struct MockBuiltinData : BuiltinFunctionsLibImpl::BuiltinData {
        using BuiltinFunctionsLibImpl::BuiltinData::func;
        using BuiltinFunctionsLibImpl::BuiltinData::module;
        MockBuiltinData(std::unique_ptr<L0::Module> &&mod, std::unique_ptr<L0::Kernel> &&ker) {
            module = std::move(mod);
            func = std::move(ker);
        }
        ~MockBuiltinData() override {
            module.release();
        }
    };
    void SetUp() {
        DeviceFixture::SetUp();
        mockDevicePtr = std::unique_ptr<MockDeviceForSpv<useImagesBuiltins, isStateless>>(new MockDeviceForSpv<useImagesBuiltins, isStateless>(device->getNEODevice(), device->getNEODevice()->getExecutionEnvironment(), driverHandle.get()));
        mockBuiltinFunctionsLibImpl.reset(new MockBuiltinFunctionsLibImpl(mockDevicePtr.get(), neoDevice->getBuiltIns()));
    }
    void TearDown() {
        mockBuiltinFunctionsLibImpl.reset();
        DeviceFixture::TearDown();
    }

    std::unique_ptr<MockBuiltinFunctionsLibImpl> mockBuiltinFunctionsLibImpl;
    std::unique_ptr<MockDeviceForSpv<useImagesBuiltins, isStateless>> mockDevicePtr;
};

template <bool useImagesBuiltins, bool isStateless>
using TestBuiltinFunctionsLibImpl = Test<BuiltinFunctionsLibFixture<useImagesBuiltins, isStateless>>;

class TestBuiltinFunctionsLibImplDefault : public TestBuiltinFunctionsLibImpl<false, false> {};
class TestBuiltinFunctionsLibImplStateless : public TestBuiltinFunctionsLibImpl<false, true> {};
class TestBuiltinFunctionsLibImplImages : public TestBuiltinFunctionsLibImpl<true, false> {};

HWTEST_F(TestBuiltinFunctionsLibImplImages, givenImageSupportThenEachBuiltinImageFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedImageBuiltins[static_cast<uint32_t>(ImageBuiltin::COUNT)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }
    if (mockDevicePtr->getHwInfo().capabilityTable.supportsImages) {
        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
            EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
            initializedImageBuiltins[builtId] = mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]->func.get();
        }

        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_EQ(initializedImageBuiltins[builtId],
                      mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        }
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplImages, givenImageSupportAndWrongIdWhenCallingBuiltinImageFunctionThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }
    if (mockDevicePtr->getHwInfo().capabilityTable.supportsImages) {
        uint32_t builtId = static_cast<uint32_t>(ImageBuiltin::COUNT) + 1;
        EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinImageKernel(static_cast<L0::ImageBuiltin>(builtId)), std::exception);
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenCallsToGetFunctionThenEachBuiltinFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedBuiltins[static_cast<uint32_t>(Builtin::COUNT)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
        initializedBuiltins[builtId] = mockBuiltinFunctionsLibImpl->builtins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(initializedBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplStateless, givenCallsToGetFunctionThenEachBuiltinFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedBuiltins[static_cast<uint32_t>(Builtin::COUNT)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
        initializedBuiltins[builtId] = mockBuiltinFunctionsLibImpl->builtins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(initializedBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenCallToBuiltinFunctionWithWrongIdThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }
    uint32_t builtId = static_cast<uint32_t>(Builtin::COUNT) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinKernel(static_cast<L0::Builtin>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImplStateless, givenCallToStatelessBuiltinFunctionWithWrongIdThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }
    uint32_t builtId = static_cast<uint32_t>(Builtin::COUNT) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinKernel(static_cast<L0::Builtin>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenCompilerInterfaceWhenCreateDeviceAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    if (device->getHwInfo().capabilityTable.supportsImages) {
        for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::COUNT); builtId++) {
            EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        }
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenIntermediateCodeForBuiltinsIsRequested) {
    struct MockDeviceForRebuildBuilins : public Mock<DeviceImp> {
        struct MockModuleForRebuildBuiltins : public ModuleImp {
            MockModuleForRebuildBuiltins(Device *device) : ModuleImp(device, nullptr, ModuleType::Builtin) {}

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
                                 ze_module_build_log_handle_t *buildLog, ModuleType type) override {
            EXPECT_EQ(desc->format, ZE_MODULE_FORMAT_IL_SPIRV);
            EXPECT_GT(desc->inputSize, 0u);
            EXPECT_NE(desc->pInputModule, nullptr);
            createModuleCalled = true;

            *module = new MockModuleForRebuildBuiltins(this);

            return ZE_RESULT_SUCCESS;
        }

        bool createModuleCalled = false;
    };

    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);
    MockDeviceForRebuildBuilins testDevice(device);
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(&testDevice, neoDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, givenNotToRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenNativeCodeForBuiltinsIsRequested) {
    struct MockDeviceForRebuildBuilins : public Mock<DeviceImp> {
        MockDeviceForRebuildBuilins(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            driverHandle = device->getDriverHandle();
            builtins = BuiltinFunctionsLib::create(this, neoDevice->getBuiltIns());
        }

        ze_result_t createModule(const ze_module_desc_t *desc,
                                 ze_module_handle_t *module,
                                 ze_module_build_log_handle_t *buildLog, ModuleType type) override {
            EXPECT_EQ(desc->format, ZE_MODULE_FORMAT_NATIVE);
            EXPECT_GT(desc->inputSize, 0u);
            EXPECT_NE(desc->pInputModule, nullptr);
            createModuleCalled = true;

            return DeviceImp::createModule(desc, module, buildLog, type);
        }

        bool createModuleCalled = false;
    };

    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(false);
    MockDeviceForRebuildBuilins testDevice(device);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(testDevicePtr, neoDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(TestBuiltinFunctionsLibImplDefault, GivenBuiltinsWhenInitializingFunctionsThenModulesWithProperTypeAreCreated) {
    struct MockDeviceWithBuilins : public Mock<DeviceImp> {
        MockDeviceWithBuilins(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
            driverHandle = device->getDriverHandle();
            builtins = BuiltinFunctionsLib::create(this, neoDevice->getBuiltIns());
        }

        ze_result_t createModule(const ze_module_desc_t *desc,
                                 ze_module_handle_t *module,
                                 ze_module_build_log_handle_t *buildLog, ModuleType type) override {

            typeCreated = type;
            EXPECT_EQ(ModuleType::Builtin, type);

            return DeviceImp::createModule(desc, module, buildLog, type);
        }

        ModuleType typeCreated = ModuleType::User;
    };

    MockDeviceWithBuilins testDevice(device);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(testDevicePtr, neoDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_EQ(ModuleType::Builtin, testDevice.typeCreated);
}

} // namespace ult
} // namespace L0
