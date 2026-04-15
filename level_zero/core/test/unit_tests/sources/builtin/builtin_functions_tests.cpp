/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
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
        using BuiltInKernelLibImpl::maxBufferCacheSize;
        using BuiltInKernelLibImpl::maxImageCacheSize;
        using BuiltInKernelLibImpl::toBufferCacheIndex;
        using BuiltInKernelLibImpl::toImageCacheIndex;
        MockBuiltInKernelLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltInKernelLibImpl(device, builtInsLib) {
            mockModule = std::make_unique<Mock<Module>>(device, nullptr);
        }
        std::unique_ptr<BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::BaseKernel baseKernel, const NEO::BuiltIn::AddressingMode &mode, const char *kernelName) override {
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
    auto mode = getDefaultBuiltInMode();

    for (uint32_t builtId = 0; builtId < mockBuiltinFunctionsLibImpl->maxImageCacheSize; builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[builtId]);
    }

    L0::Kernel *initializedImageBuiltIns[static_cast<uint32_t>(ImageBuiltIn::count)];
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        auto func = static_cast<L0::ImageBuiltIn>(builtId);
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getImageFunction(func, mode));
        auto cacheIdx = mockBuiltinFunctionsLibImpl->toImageCacheIndex(func, mode);
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->imageBuiltins[cacheIdx]);
        initializedImageBuiltIns[builtId] = mockBuiltinFunctionsLibImpl->imageBuiltins[cacheIdx]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        auto func = static_cast<L0::ImageBuiltIn>(builtId);
        EXPECT_EQ(initializedImageBuiltIns[builtId],
                  mockBuiltinFunctionsLibImpl->getImageFunction(func, mode));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenImageSupportAndWrongIdWhenCallingBuiltinImageFunctionThenExceptionIsThrown) {
    auto mode = getDefaultBuiltInMode();

    uint32_t builtId = static_cast<uint32_t>(ImageBuiltIn::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinImageKernel(static_cast<L0::ImageBuiltIn>(builtId), mode), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallsToGetFunctionThenEachBuiltinFunctionsIsLoadedOnlyOnce) {
    auto mode = getDefaultBuiltInMode();

    for (uint32_t builtId = 0; builtId < mockBuiltinFunctionsLibImpl->maxBufferCacheSize; builtId++) {
        EXPECT_EQ(nullptr, mockBuiltinFunctionsLibImpl->builtins[builtId]);
    }

    L0::Kernel *initializedBuiltins[static_cast<uint32_t>(BufferBuiltIn::count)];
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        auto func = static_cast<L0::BufferBuiltIn>(builtId);
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->getFunction(func, mode));
        auto cacheIdx = mockBuiltinFunctionsLibImpl->toBufferCacheIndex(func, mode);
        EXPECT_NE(nullptr, mockBuiltinFunctionsLibImpl->builtins[cacheIdx]);
        initializedBuiltins[builtId] = mockBuiltinFunctionsLibImpl->builtins[cacheIdx]->func.get();
    }

    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        auto func = static_cast<L0::BufferBuiltIn>(builtId);
        EXPECT_EQ(initializedBuiltins[builtId],
                  mockBuiltinFunctionsLibImpl->getFunction(func, mode));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCallToBuiltinFunctionWithWrongIdThenExceptionIsThrown) {
    auto mode = getDefaultBuiltInMode();

    uint32_t builtId = static_cast<uint32_t>(BufferBuiltIn::count) + 1;
    EXPECT_THROW(mockBuiltinFunctionsLibImpl->initBuiltinKernel(static_cast<L0::BufferBuiltIn>(builtId), mode), std::exception);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, whenCreateBuiltinFunctionsLibThenImmediateFillIsLoaded) {
    struct MockBuiltInKernelLibImpl : public BuiltInKernelLibImpl {
        using BuiltInKernelLibImpl::BuiltInKernelLibImpl;
        using BuiltInKernelLibImpl::builtins;
        using BuiltInKernelLibImpl::ensureInitCompletion;
        using BuiltInKernelLibImpl::initAsyncComplete;
        using BuiltInKernelLibImpl::maxBufferCacheSize;
        using BuiltInKernelLibImpl::toBufferCacheIndex;
    };

    EXPECT_TRUE(mockBuiltinFunctionsLibImpl->initAsyncComplete);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;
    MockBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    EXPECT_FALSE(lib.initAsyncComplete);
    lib.ensureInitCompletion();
    EXPECT_TRUE(lib.initAsyncComplete);
    auto defaultMode = getDefaultBuiltInMode();
    auto expectedCacheIndex = lib.toBufferCacheIndex(BufferBuiltIn::fillBufferImmediate, defaultMode);

    for (uint32_t builtId = 0; builtId < lib.maxBufferCacheSize; builtId++) {
        if (builtId == expectedCacheIndex) {
            EXPECT_NE(nullptr, lib.builtins[builtId]);
        } else {
            EXPECT_EQ(nullptr, lib.builtins[builtId]);
        }
    }
    uint32_t builtId = static_cast<uint32_t>(BufferBuiltIn::count) + 1;
    EXPECT_THROW(lib.initBuiltinKernel(static_cast<L0::BufferBuiltIn>(builtId), defaultMode), std::exception);

    /* std::async may create a detached thread - completion of the scheduled task can be ensured,
       but there is no way to ensure that actual OS thread exited and its resources are freed */
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::TURN_OFF_LEAK_DETECTION;
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenBindlessBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    auto bindlessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferBytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("copyBufferToBufferBytesSingle", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferToBufferMiddle, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToBufferMiddleRegion", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferToBufferSide, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToBufferSideRegion", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferImmediate, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::fillBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("FillBufferImmediate", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferImmediateLeftOver, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::fillBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("FillBufferImmediateLeftOver", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferSSHOffset, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::fillBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("FillBufferSSHOffset", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferMiddle, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::fillBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("FillBufferMiddle", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::fillBufferRightLeftover, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::fillBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("FillBufferRightLeftover", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenBindlessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    auto bindlessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImageRegion, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImageToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToImage3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenBindlessBufferRectBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    auto bindlessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes2d, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferRect, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferRectBytes2d", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes3d, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferRect, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferRectBytes3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenStatelessBufferRectBuiltinsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {

    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    auto statelessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, true);

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes2d, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferRect, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferRectBytes2d", lib.kernelNamePassed.c_str());

    lib.initBuiltinKernel(L0::BufferBuiltIn::copyBufferRectBytes3d, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferRect, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferRectBytes3d", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenStatelessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());
    auto statelessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, true);

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenAlignedImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    auto defaultMode = getDefaultBuiltInMode();
    auto statelessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, true);
    auto bindlessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned, defaultMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    auto wideStatelessMode = statelessMode;
    wideStatelessMode.wideMode = true;
    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned, wideStatelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    auto wideBindlessMode = bindlessMode;
    wideBindlessMode.wideMode = true;
    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16BytesAligned, wideBindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned, defaultMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned, wideStatelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16BytesAligned, wideBindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16BytesAligned", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceAndImageSupportedThenBuiltinsImageFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(ImageBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getImageFunction(static_cast<L0::ImageBuiltIn>(builtId), mode));
    }
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenCompilerInterfaceWhenCreateDeviceThenBuiltinsFunctionsAreLoaded) {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(new NEO::MockCompilerInterfaceSpirv());
    std::unique_ptr<L0::Device> testDevice(Device::create(device->getDriverHandle(), neoDevice, false, &returnValue));

    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        EXPECT_NE(nullptr, testDevice->getBuiltinFunctionsLib()->getFunction(static_cast<L0::BufferBuiltIn>(builtId), mode));
    }
}

struct BuiltInTestsL0 : Test<NEO::DeviceFixture> {
    NEO::BuiltIn::AddressingMode getDefaultBuiltInMode() const {
        return pDevice->getCompilerProductHelper().getDefaultBuiltInAddressingMode(
            NEO::ApiSpecificConfig::getBindlessMode(*pDevice));
    }
};

HWTEST_F(BuiltInTestsL0, givenRebuildPrecompiledKernelsDebugFlagWhenInitFuctionsThenIntermediateCodeForBuiltinsIsRequested) {
    pDevice->incRefInternal();
    DebugManagerStateRestore dgbRestorer;
    NEO::debugManager.flags.RebuildPrecompiledKernels.set(true);
    MockDeviceForBuiltinTests testDevice(pDevice);
    testDevice.builtins.reset(new BuiltInKernelLibImpl(&testDevice, pDevice->getBuiltIns()));
    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId), mode);
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
    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId), mode);
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
}

HWTEST_F(BuiltInTestsL0, givenBuiltinsWhenInitializingFunctionsThenModulesWithProperTypeAreCreated) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    testDevice.builtins.reset(new BuiltInKernelLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId), mode);
    }

    EXPECT_EQ(ModuleType::builtin, testDevice.typeCreated);
}

HWTEST_F(BuiltInTestsL0, givenDeviceWithUnregisteredBinaryBuiltinWhenGettingBuiltinKernelThenOnlyIntermediateFormatIsAvailable) {
    pDevice->incRefInternal();
    MockDeviceForBuiltinTests testDevice(pDevice);
    L0::Device *testDevicePtr = &testDevice;
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->ipVersion.value += 0xdead;
    testDevice.builtins.reset(new BuiltInKernelLibImpl(testDevicePtr, pDevice->getBuiltIns()));
    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(L0::BufferBuiltIn::count); builtId++) {
        testDevice.formatForModule = {};
        ASSERT_NE(nullptr, testDevice.getBuiltinFunctionsLib()->getFunction(static_cast<L0::BufferBuiltIn>(builtId), mode));
        EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, testDevice.formatForModule);
    }
}

HWTEST_F(BuiltInTestsL0, givenOneApiPvcSendWarWaEnvFalseWhenGettingBuiltinThenIntermediateFormatIsUsed) {
    pDevice->incRefInternal();
    pDevice->getExecutionEnvironment()->setOneApiPvcWaEnv(false);

    MockDeviceForBuiltinTests testDevice(pDevice);
    testDevice.builtins.reset(new BuiltInKernelLibImpl(&testDevice, pDevice->getBuiltIns()));
    auto mode = getDefaultBuiltInMode();
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(BufferBuiltIn::count); builtId++) {
        testDevice.getBuiltinFunctionsLib()->initBuiltinKernel(static_cast<BufferBuiltIn>(builtId), mode);
    }

    EXPECT_TRUE(testDevice.createModuleCalled);
    EXPECT_EQ(ZE_MODULE_FORMAT_IL_SPIRV, testDevice.formatForModule);
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenWideStatelessBindlessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    auto bindlessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(true, true);
    bindlessMode.wideMode = true;

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6Bytes, bindlessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

HWTEST_F(TestBuiltinFunctionsLibImpl, givenWideStatelessImageBuiltInsWhenInitBuiltinKernelThenCorrectArgumentsArePassed) {
    MockCheckPassedArgumentsBuiltInKernelLibImpl lib(device, device->getNEODevice()->getBuiltIns());

    auto statelessMode = NEO::BuiltIn::AddressingMode::getDefaultMode(false, true);
    statelessMode.wideMode = true;

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d16Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d2Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d4Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyBufferToImage3d8Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyBufferToImage3d, lib.baseKernelPassed);
    EXPECT_STREQ("CopyBufferToImage3d8Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer16Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer16Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer2Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer2Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer4Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer4Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBufferBytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBufferBytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer3Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer3Bytes", lib.kernelNamePassed.c_str());

    lib.initBuiltinImageKernel(L0::ImageBuiltIn::copyImage3dToBuffer6Bytes, statelessMode);
    EXPECT_EQ(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, lib.baseKernelPassed);
    EXPECT_STREQ("CopyImage3dToBuffer6Bytes", lib.kernelNamePassed.c_str());
}

} // namespace ult
} // namespace L0
