/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/memory_management.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

using BuiltinLibAsyncInitMtTest = Test<DeviceFixture>;

TEST_F(BuiltinLibAsyncInitMtTest, givenAsyncInitEnabledWhenBuiltinLibInitializedThenOnlyImmediateFillKernelIsLoaded) {
    struct MockBuiltInKernelLibImpl : public BuiltInKernelLibImpl {
        using BuiltInKernelLibImpl::BuiltInKernelLibImpl;
        using BuiltInKernelLibImpl::builtins;
        using BuiltInKernelLibImpl::ensureInitCompletion;
        using BuiltInKernelLibImpl::initAsyncComplete;
        using BuiltInKernelLibImpl::maxBufferCacheSize;
        using BuiltInKernelLibImpl::toBufferCacheIndex;
    };

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useinitBuiltinsAsyncEnabled = true;
    MemoryManagement::pendingDetachedThreadCleanup = true;
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
}

TEST(DriverHandleBuiltinInitMtTest, givenAsyncInitEnabledWhenDriverHandleCreatedThenBuiltinsInitIsComplete) {
    VariableBackup<NEO::UltHwConfig> backup(&NEO::ultHwConfig);
    NEO::ultHwConfig.useinitBuiltinsAsyncEnabled = true;

    ze_result_t returnValue;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();

    NEO::MockDevice *neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    L0EnvVariables envVariables = {};

    auto driverHandle = whiteboxCast(static_cast<::L0::DriverHandle *>(DriverHandle::create(std::move(devices), envVariables, &returnValue)));
    EXPECT_NE(nullptr, driverHandle);

    L0::Device *device = driverHandle->devices[0];
    auto builtinFunctionsLib = device->getBuiltinFunctionsLib();

    if (builtinFunctionsLib) {
        auto builtinsLibIpl = static_cast<MockBuiltInKernelLibImpl *>(builtinFunctionsLib);
        EXPECT_TRUE(builtinsLibIpl->initAsyncComplete);
    }

    delete driverHandle;
}

} // namespace ult
} // namespace L0
