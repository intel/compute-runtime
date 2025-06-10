/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/memory_management.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/mock_compiler_interface_spirv.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
class BuiltinFunctionsLibFixture : public DeviceFixture {
  public:
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {
        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::ensureInitCompletion;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        using BuiltinFunctionsLibImpl::initAsyncComplete;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
        }
        std::unique_ptr<BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
            std::unique_ptr<Kernel> mockKernel(new Mock<::L0::KernelImp>());

            return std::unique_ptr<BuiltinData>(new BuiltinData{mockModule.get(), std::move(mockKernel)});
        }
        std::unique_ptr<Module> mockModule;
    };

    void setUp() {
        DeviceFixture::setUp();
        mockDevicePtr = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), driverHandle.get()));
        mockBuiltinFunctionsLibImpl.reset(new MockBuiltinFunctionsLibImpl(mockDevicePtr.get(), neoDevice->getBuiltIns()));
        mockBuiltinFunctionsLibImpl->ensureInitCompletion();
        EXPECT_TRUE(mockBuiltinFunctionsLibImpl->initAsyncComplete);
    }
    void tearDown() {
        mockBuiltinFunctionsLibImpl.reset();
        DeviceFixture::tearDown();
    }

    std::unique_ptr<MockBuiltinFunctionsLibImpl> mockBuiltinFunctionsLibImpl;
    std::unique_ptr<MockDeviceForSpv<false, false>> mockDevicePtr;
};

using TestBuiltinFunctionsLibImpl = Test<BuiltinFunctionsLibFixture>;

HWTEST_F(TestBuiltinFunctionsLibImpl, givenImageSupportThenEachBuiltinImageFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedImageBuiltins[static_cast<uint32_t>(ImageBuiltin::count)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::count); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
        initializedImageBuiltins[builtId] = mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::count); builtId++) {
        EXPECT_EQ(initializedImageBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenImageSupportAndWrongIdWhenCallingBuiltinImageFunctionThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }

    uint32_t builtId = static_cast<uint32_t>(ImageBuiltin::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinImageKernel(static_cast<L0::ImageBuiltin>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallsToGetFunctionThenEachBuiltinFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedBuiltins[static_cast<uint32_t>(Builtin::count)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
        initializedBuiltins[builtId] = mockBuiltinFunctionsLibImpl->builtins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        EXPECT_EQ(initializedBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallToBuiltinFunctionWithWrongIdThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }
    uint32_t builtId = static_cast<uint32_t>(Builtin::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinKernel(static_cast<L0::Builtin>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, whenCreateBuiltinFunctionsLibThenImmediateFillIsLoaded) {
    struct MockBuiltinFunctionsLibImpl : public BuiltinFunctionsLibImpl {
        using BuiltinFunctionsLibImpl::BuiltinFunctionsLibImpl;
        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::ensureInitCompletion;
        using BuiltinFunctionsLibImpl::initAsyncComplete;
    };

    EXPECT_TRUE(mockBuiltinFunctionsLibImpl->initAsyncComplete);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;
    MockBuiltinFunctionsLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    EXPECT_FALSE(lib.initAsyncComplete);
    lib.ensureInitCompletion();
    EXPECT_TRUE(lib.initAsyncComplete);
    const auto &compilerProductHelper = device->getCompilerProductHelper();
    auto expectedInitFillBuiltin = Builtin::count;
    if (compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo())) {
        expectedInitFillBuiltin = Builtin::fillBufferImmediateStatelessHeapless;
    } else if (compilerProductHelper.isForceToStatelessRequired()) {
        expectedInitFillBuiltin = Builtin::fillBufferImmediateStateless;
    } else {
        expectedInitFillBuiltin = Builtin::fillBufferImmediate;
    }
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        if (builtId == static_cast<uint32_t>(expectedInitFillBuiltin)) {
            EXPECT_NE(nullptr, lib.builtins[builtId]);
        } else {
            EXPECT_EQ(nullptr, lib.builtins[builtId]);
        }
    }
    uint32_t builtId = static_cast<uint32_t>(Builtin::count) + 1;
    EXPECT_THROW(lib.initBuiltinKernel(static_cast<L0::Builtin>(builtId)), std::exception);

    /* std::async may create a detached thread - completion of the scheduled task can be ensured,
       but there is no way to ensure that actual OS thread exited and its resources are freed */
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::TURN_OFF_LEAK_DETECTION;
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenHeaplessBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltinFunctionsLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinKernel(L0::Builtin::copyBufferBytesStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("copyBufferToBufferBytesSingleStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::copyBufferToBufferMiddleStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToBufferMiddleRegionStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::copyBufferToBufferSideStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToBufferSideRegionStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferImmediateStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferImmediateStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferImmediateLeftOverStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferImmediateLeftOverStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferSSHOffsetStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferSSHOffsetStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferSSHOffsetStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferSSHOffsetStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferMiddleStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferMiddleStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::Builtin::fillBufferRightLeftoverStatelessHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::fillBufferStatelessHeapless, lib.builtinPassed);
    EXPECT_STREQ("FillBufferRightLeftoverStateless", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenHeaplessImageBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltinFunctionsLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyBufferToImage3d16BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToImage3dHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyBufferToImage3d2BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToImage3dHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToImage3d2BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyBufferToImage3d4BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToImage3dHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToImage3d4BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyBufferToImage3d8BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyBufferToImage3dHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyBufferToImage3d8BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBuffer16BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBuffer2BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBuffer4BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBufferBytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBuffer3BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImage3dToBuffer6BytesHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImage3dToBufferHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6BytesStateless", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltin::copyImageRegionHeapless);
    EXPECT_EQ(NEO::EBuiltInOps::copyImageToImage3dHeapless, lib.builtinPassed);
    EXPECT_STREQ("CopyImage3dToImage3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltin::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltin>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::Builtin>(builtId)));
    }
}

using BuiltInTestsL0 = Test<NEO::DeviceFixture>;

HWTEST_F(BuiltInTestsL0, givenRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenIntermediateCodeForBuiltinsIsRequested) {
    pDevice->incRefInternal();
    DebugManagerStateRestore dgbRestorer;
    NEO::debugManager.flags.RebuildPrecompiledKernels.set(true);
    MockDeviceForBuiltinTests testDevice(pDevice);
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(&testDevice, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(BuiltInTestsL0, givenNotToRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenNativeCodeForBuiltinsIsRequested) {
    pDevice->incRefInternal();
    DebugManagerStateRestore dgbRestorer;
    NEO::debugManager.flags.RebuildPrecompiledKernels.set(false);
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(BuiltInTestsL0, givenBuiltinsWhenInitializingFunctionsThenModulesWithProperTypeAreCreated) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<Builtin>(builtId));
    }

    EXPECT_EQ(ModuleType::builtin, testDevice.typeCreated);
}

HWTEST_F(BuiltInTestsL0, givenDeviceWithUnregisteredBinaryBuiltinWhenGettingBuiltinKernelThenOnlyIntermediateFormatIsAvailable) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->ipVersion.value += 0xdead;
    testDevice.builtins.reset(new BuiltinFunctionsLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(L0::Builtin::count); builtId++) {
        testDevice.formatForModule = {};
        ASSERT_NE(nullptr, testDevice.getBuiltinFunctionsLib()->getFunction(static_cast<L0::Builtin>(builtId)));
        EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, testDevice.formatForModule);
    }
}

} // namespace ult
} // namespace L0
