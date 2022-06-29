/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/root_device.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

struct TestDeviceUuid : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
    DebugManagerStateRestore restorer;
};

HWTEST2_F(TestDeviceUuid, GivenCorrectTelemetryNodesAreAvailableWhenRetrievingDeviceAndSubDevicePropertiesThenCorrectUuidIsReceived, IsXEHP) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/dev/char/226:128", "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128"},
            {"/sys/class/intel_pmt/telem3", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem3/"},
            {"/sys/class/intel_pmt/telem1", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
            {"/sys/class/intel_pmt/telem2", "./../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:02.0/0000:3c:00.1/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
        };

        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "/sys/class/intel_pmt/telem1/guid",
            "/sys/class/intel_pmt/telem1/offset",
            "/sys/class/intel_pmt/telem1/telem",
        };

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            // skipping "0"
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"/sys/class/intel_pmt/telem1/guid", "0xfdc76195"},
            {"/sys/class/intel_pmt/telem1/offset", "0\n"},
            {"/sys/class/intel_pmt/telem1/telem", "dummy"},
        };

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            if (supportedFiles[fd].second == "dummy") {
                uint64_t data = 0xFEEDBEADDEABDEEF;
                memcpy(buf, &data, sizeof(data));
                return sizeof(data);
            }
            memcpy(buf, supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });
    DebugManager.flags.EnableChipsetUniqueUUID.set(1);
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    DrmMockResources *drmMock = nullptr;

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    auto osInterface = new OSInterface();
    drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    std::vector<std::string> pciPaths = {
        "0000:3a:00.0"};
    drmMock->setPciPath(pciPaths[0].c_str());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

    neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    uint64_t expectedVal = 0xFEEDBEADDEABDEEF;
    ze_device_properties_t deviceProps;
    deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getProperties(&deviceProps));
    EXPECT_TRUE(0 == std::memcmp(deviceProps.uuid.id, &expectedVal, sizeof(expectedVal)));

    uint32_t subdeviceCount = neoDevice->getNumGenericSubDevices();
    std::vector<ze_device_handle_t> subdevices;
    subdevices.resize(subdeviceCount);
    device->getSubDevices(&subdeviceCount, subdevices.data());

    uint8_t expectedUuid[16] = {0};
    std::memcpy(expectedUuid, &expectedVal, sizeof(uint64_t));
    expectedUuid[15] = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, static_cast<Device *>(subdevices[0])->getProperties(&deviceProps));
    EXPECT_TRUE(0 == std::memcmp(deviceProps.uuid.id, expectedUuid, sizeof(expectedUuid)));
    expectedUuid[15] = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, static_cast<Device *>(subdevices[1])->getProperties(&deviceProps));
    EXPECT_TRUE(0 == std::memcmp(deviceProps.uuid.id, expectedUuid, sizeof(expectedUuid)));
}

TEST_F(TestDeviceUuid, GivenEnableChipsetUniqueUuidIsSetWhenOsInterfaceIsNotSetThenUuidOfFallbackPathIsReceived) {

    DebugManager.flags.EnableChipsetUniqueUUID.set(1);
    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_device_properties_t deviceProperties, devicePropertiesBefore;
    deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    memset(&deviceProperties.uuid, std::numeric_limits<int>::max(), sizeof(deviceProperties.uuid));
    devicePropertiesBefore = deviceProperties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getProperties(&deviceProperties));
    EXPECT_NE(0, memcmp(&deviceProperties.uuid, &devicePropertiesBefore.uuid, sizeof(devicePropertiesBefore.uuid)));
}

} // namespace ult
} // namespace L0