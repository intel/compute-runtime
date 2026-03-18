/*
 * Copyright (C) 2020-2026 Intel Corporation
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
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
class BuiltInKernelLibFixture : public DeviceFixture {
  public:
    struct MockBuiltInKernelLibImpl : BuiltInKernelLibImpl {
        using BuiltInKernelLibImpl::builtins;
        using BuiltInKernelLibImpl::ensureInitCompletion;
        using BuiltInKernelLibImpl::getFunction;
        using BuiltInKernelLibImpl::imageBuiltins;
        using BuiltInKernelLibImpl::initAsyncComplete;
        MockBuiltInKernelLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltInKernelLibImpl(device, builtInsLib) {
            mockModule = std::make_unique<Mock<Module>>(device, nullptr);
        }
        std::unique_ptr<BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::Group builtInGroup, const char *kernelName) override {
            auto mockKernel = std::make_unique<Mock<::L0::KernelImp>>();

            return std::make_unique<BuiltInKernelData>(mockModule.get(), std::move(mockKernel));
        }
        std::unique_ptr<Module> mockModule;
    };

    void setUp() {
        DeviceFixture::setUp();
        mockDevicePtr = std::make_unique<MockDeviceForSpv>(device->getNEODevice(), driverHandle.get());
        mockBuiltinFunctionsLibImpl = std::make_unique<MockBuiltInKernelLibImpl>(mockDevicePtr.get(), neoDevice->getBuiltIns());
        mockBuiltinFunctionsLibImpl->ensureInitCompletion();
        EXPECT_TRUE(mockBuiltinFunctionsLibImpl->initAsyncComplete);
    }
    void tearDown() {
        mockBuiltinFunctionsLibImpl.reset();
        DeviceFixture::tearDown();
    }

    std::unique_ptr<MockBuiltInKernelLibImpl> mockBuiltinFunctionsLibImpl;
    std::unique_ptr<MockDeviceForSpv> mockDevicePtr;
};

using TestBuiltinFunctionsLibImpl = Test<BuiltInKernelLibFixture>;

HWTEST_F(TestBuiltinFunctionsLibImpl, givenImageSupportThenEachBuiltinImageFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedImageBuiltIns[static_cast<uint32_t>(ImageBuiltIn::count)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltIn>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
        initializedImageBuiltIns[builtId] = mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_EQ(initializedImageBuiltIns[builtId],
                  mockBuiltinFunctionsLibImpl->getImageFunction(static_cast<L0::ImageBuiltIn>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenImageSupportAndWrongIdWhenCallingBuiltinImageFunctionThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }

    uint32_t builtId = static_cast<uint32_t>(ImageBuiltIn::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinImageKernel(static_cast<L0::ImageBuiltIn>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallsToGetFunctionThenEachBuiltinFunctionsIsLoadedOnlyOnce) {
    L0::Kernel *initializedBuiltins[static_cast<uint32_t>(BufferBuiltIn::count)];

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::BufferBuiltIn>(builtId)));
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
        initializedBuiltins[builtId] = mockBuiltinFunctionsLibImpl->builtins[builtId]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_EQ(initializedBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getFunction(static_cast<L0::BufferBuiltIn>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallToBuiltinFunctionWithWrongIdThenExceptionIsThrown) {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }
    uint32_t builtId = static_cast<uint32_t>(BufferBuiltIn::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinKernel(static_cast<L0::BufferBuiltIn>(builtId)), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, whenCreateBuiltinFunctionsLibThenImmediateFillIsLoaded) {
    struct MockBuiltInKernelLibImpl : public BuiltInKernelLibImpl {
        using BuiltInKernelLibImpl::BuiltInKernelLibImpl;
        using BuiltInKernelLibImpl::builtins;
        using BuiltInKernelLibImpl::ensureInitCompletion;
        using BuiltInKernelLibImpl::initAsyncComplete;
    };

    EXPECT_TRUE(mockBuiltinFunctionsLibImpl->initAsyncComplete);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;
    MockBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    EXPECT_FALSE(lib.initAsyncComplete);
    lib.ensureInitCompletion();
    EXPECT_TRUE(lib.initAsyncComplete);
    const auto &compilerProductHelper = device->getCompilerProductHelper();
    auto expectedInitFillBuiltin = BufferBuiltIn::count;
    if (compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo())) {
        expectedInitFillBuiltin = BufferBuiltIn::fillBufferImmediateStatelessHeapless;
    } else if (compilerProductHelper.isForceToStatelessRequired()) {
        expectedInitFillBuiltin = BufferBuiltIn::fillBufferImmediateStateless;
    } else {
        expectedInitFillBuiltin = BufferBuiltIn::fillBufferImmediate;
    }
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        if (builtId == static_cast<uint32_t>(expectedInitFillBuiltin)) {
            EXPECT_NE(nullptr, lib.builtins[builtId]);
        } else {
            EXPECT_EQ(nullptr, lib.builtins[builtId]);
        }
    }
    uint32_t builtId = static_cast<uint32_t>(BufferBuiltIn::count) + 1;
    EXPECT_THROW(lib.initBuiltinKernel(static_cast<L0::BufferBuiltIn>(builtId)), std::exception);

    /* std::async may create a detached thread - completion of the scheduled task can be ensured,
       but there is no way to ensure that actual OS thread exited and its resources are freed */
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::TURN_OFF_LEAK_DETECTION;
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenHeaplessBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferBytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("copyBufferToBufferBytesSingle", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferToBufferMiddleStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToBufferMiddleRegion", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferToBufferSideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToBufferSideRegion", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferImmediateStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferImmediate", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferImmediateLeftOverStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferImmediateLeftOver", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferSSHOffsetStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferSSHOffset", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferSSHOffsetStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferSSHOffset", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferMiddleStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferMiddle", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferRightLeftoverStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::fillBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("FillBufferRightLeftover", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenHeaplessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6BytesStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImageRegionHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImageToImage3dHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToImage3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenHeaplessBufferRectBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes2dStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferRectStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferRectBytes2d", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes3dStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferRectStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferRectBytes3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenStatelessBufferRectBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes2dStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferRectStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferRectBytes2d", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes3dStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferRectStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferRectBytes3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenStatelessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6BytesStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenAlignedImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3d, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAlignedStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAlignedWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAlignedStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAlignedWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBuffer, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAlignedStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAlignedWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAlignedStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAlignedWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltIn>(builtId)));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::BufferBuiltIn>(builtId)));
    }
}

using BuiltInTestsL0 = Test<NEO::DeviceFixture>;

HWTEST_F(BuiltInTestsL0, givenRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenIntermediateCodeForBuiltinsIsRequested) {
    pDevice->incRefInternal();
    DebugManagerStateRestore dgbRestorer;
    NEO::debugManager.flags.RebuildPrecompiledKernels.set(true);
    MockDeviceForBuiltinTests testDevice(pDevice);
    testDevice.builtins.reset(new BuiltInKernelLibImpl(&testDevice, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(BuiltInTestsL0, givenNotToRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenNativeCodeForBuiltinsIsRequested) {
    pDevice->incRefInternal();
    DebugManagerStateRestore dgbRestorer;
    NEO::debugManager.flags.RebuildPrecompiledKernels.set(false);
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltInKernelLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(BuiltInTestsL0, givenBuiltinsWhenInitializingFunctionsThenModulesWithProperTypeAreCreated) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltInKernelLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId));
    }

    EXPECT_EQ(ModuleType::builtin, testDevice.typeCreated);
}

HWTEST_F(BuiltInTestsL0, givenDeviceWithUnregisteredBinaryBuiltinWhenGettingBuiltinKernelThenOnlyIntermediateFormatIsAvailable) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->ipVersion.value += 0xdead;
    testDevice.builtins.reset(new BuiltInKernelLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(L0::BufferBuiltIn::count); builtId++) {
        testDevice.formatForModule = {};
        ASSERT_NE(nullptr, testDevice.getBuiltinFunctionsLib()->getFunction(static_cast<L0::BufferBuiltIn>(builtId)));
        EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, testDevice.formatForModule);
    }
}

HWTEST_F(BuiltInTestsL0, givenOneApiPvcSendWarWaEnvFalseWhenGettingBuiltinThenIntermediateFormatIsUsed) {
    pDevice->incRefInternal();
    pDevice->getExecutionEnvironment()->setOneApiPvcWaEnv(false);

    MockDeviceForBuiltinTests testDevice(pDevice);
    testDevice.builtins.reset(new BuiltInKernelLibImpl(&testDevice, pDevice->getBuiltIns()));
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId));
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
    EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, testDevice.formatForModule);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenWideStatelessHeaplessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6BytesWideStatelessHeapless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenWideStatelessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyBufferToImage3dWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6BytesWideStateless);
    EXPECT_EQ(NEO::BuiltIn::Group::copyImage3dToBufferWideStateless, lib.builtInGroupPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

} // namespace ult
} // namespace L0
