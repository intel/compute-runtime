/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/device/sysman_hw_device_id.h"
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST(sysmanHwDeviceIdTest, whenOpenFileDescriptorIsCalledMultipleTimesThenOpenIsCalledOnlyOnce) {

    L0::Sysman::SysmanHwDeviceIdDrm hwDeviceId(-1, "mockPciPath", "mockDevNodePath");
    static uint32_t openCallCount = 0;

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        openCallCount++;
        return 1;
    });

    openCallCount = 0;

    auto instance1 = hwDeviceId.getSingleInstance();
    EXPECT_EQ(openCallCount, 1u);
    auto instance2 = hwDeviceId.getSingleInstance();
    EXPECT_EQ(openCallCount, 1u);
}

TEST(sysmanHwDeviceIdTest, whenOpenFileDescriptorIsCalledMultipleTimesThenCloseIsCalledOnlyAfterAllReferencesAreClosed) {

    L0::Sysman::SysmanHwDeviceIdDrm hwDeviceId(-1, "mockPciPath", "mockDevNodePath");
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    const auto referenceCloseCount = NEO::SysCalls::closeFuncCalled;
    {
        auto instance1 = hwDeviceId.getSingleInstance();
        EXPECT_EQ(NEO::SysCalls::closeFuncCalled, referenceCloseCount);
        {
            auto instance2 = hwDeviceId.getSingleInstance();
            {
                auto instance3 = hwDeviceId.getSingleInstance();
            }
            EXPECT_EQ(NEO::SysCalls::closeFuncCalled, referenceCloseCount);
        }
        EXPECT_EQ(NEO::SysCalls::closeFuncCalled, referenceCloseCount);
    }
    EXPECT_EQ(NEO::SysCalls::closeFuncCalled, referenceCloseCount + 1u);
}

TEST(sysmanHwDeviceIdTest, givenDrmHwDeviceIdWithOpenFileDescriptorWhenSysmanHwDeviceIdIsCreatedThenFileDescriptorIsMovedToSysmanHwDeviceId) {

    constexpr int discoveredFileDescriptor = 0x1234;
    std::unique_ptr<NEO::HwDeviceId> hwDeviceId = std::make_unique<NEO::HwDeviceIdDrm>(discoveredFileDescriptor, "mockPciPath", "mockDevNodePath");
    auto sysmanHwDeviceId = createSysmanHwDeviceId(hwDeviceId);

    EXPECT_EQ(-1, static_cast<NEO::HwDeviceIdDrm *>(hwDeviceId.get())->getFileDescriptor());
    EXPECT_EQ(discoveredFileDescriptor, static_cast<NEO::HwDeviceIdDrm *>(sysmanHwDeviceId.get())->getFileDescriptor());
    EXPECT_STREQ("mockPciPath", static_cast<NEO::HwDeviceIdDrm *>(sysmanHwDeviceId.get())->getPciPath());
    EXPECT_STREQ("mockDevNodePath", static_cast<NEO::HwDeviceIdDrm *>(sysmanHwDeviceId.get())->getDeviceNode());
}

TEST(sysmanHwDeviceIdTest, givenDrmHwDeviceIdWithOpenFileDescriptorWhenSysmanHwDeviceIdIsUsedAndBothAreDestroyedThenFileDescriptorIsClosedOnlyOnce) {

    constexpr int discoveredFileDescriptor = 0x1234;
    static uint32_t discoveredFileDescriptorCloseCount = 0;
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
        if (fileDescriptor == discoveredFileDescriptor) {
            discoveredFileDescriptorCloseCount++;
        }
        return 0;
    });
    discoveredFileDescriptorCloseCount = 0;

    {
        std::unique_ptr<NEO::HwDeviceId> hwDeviceId = std::make_unique<NEO::HwDeviceIdDrm>(discoveredFileDescriptor, "mockPciPath", "mockDevNodePath");
        auto sysmanHwDeviceId = createSysmanHwDeviceId(hwDeviceId);
        {
            auto instance = static_cast<SysmanHwDeviceIdDrm *>(sysmanHwDeviceId.get())->getSingleInstance();
            EXPECT_EQ(discoveredFileDescriptor, instance.getFileDescriptor());
        }
        EXPECT_EQ(1u, discoveredFileDescriptorCloseCount);
    }
    EXPECT_EQ(1u, discoveredFileDescriptorCloseCount);
}

TEST(sysmanHwDeviceIdTest, whenCreatingSysmanHwDeviceIdforWddmSystemThenSameHwDeviceIdIsReturned) {

    std::unique_ptr<NEO::HwDeviceId> hwDeviceId = std::make_unique<NEO::HwDeviceId>(NEO::DriverModelType::wddm);
    auto tempHwDeviceId = hwDeviceId.get();
    auto sysmanHwDeviceId = createSysmanHwDeviceId(hwDeviceId);
    EXPECT_EQ(tempHwDeviceId, sysmanHwDeviceId.get());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
