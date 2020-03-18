/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/debug_registry_reader.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/os_interface/windows/registry_reader_tests.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

class DriverInfoDeviceTest : public ::testing::Test {
  public:
    void SetUp() {
        hwInfo = platformDevices[0];
        commandStreamReceiverCreateFunc = commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];
        commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily] = createMockCommandStreamReceiver;
    }

    void TearDown() {
        commandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily] = commandStreamReceiverCreateFunc;
    }

    CommandStreamReceiverCreateFunc commandStreamReceiverCreateFunc;
    const HardwareInfo *hwInfo;
};

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto csr = new MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex);
    if (!executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface) {
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0]);
        wddm->init();
        executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->get()->setWddm(wddm);
    }

    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface.get());
    return csr;
}

class MockDriverInfoWindows : public DriverInfoWindows {

  public:
    using DriverInfoWindows::DriverInfoWindows;
    using DriverInfoWindows::path;
    using DriverInfoWindows::registryReader;

    const char *getRegistryReaderRegKey() {
        return reader->getRegKey();
    }
    TestedRegistryReader *reader = nullptr;

    static MockDriverInfoWindows *create(std::string path) {

        auto result = new MockDriverInfoWindows("");
        result->reader = new TestedRegistryReader(path);
        result->registryReader.reset(result->reader);

        return result;
    };
};

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWhenCorrectOSInterfaceThenCreateDriverInfo) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));

    EXPECT_NE(nullptr, device->driverInfo.get());
}

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWithoutCorrectOSInterfaceThenDontCreateDriverInfo) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));

    EXPECT_EQ(nullptr, device->driverInfo.get());
}

class RegistryReaderMock : public SettingsReader {
  public:
    std::string nameString;
    std::string versionString;
    std::string getSetting(const char *settingName, const std::string &value) {
        std::string key(settingName);
        if (key == "HardwareInformation.AdapterString") {
            properNameKey = true;
        } else if (key == "DriverVersion") {
            properVersionKey = true;
        }
        return value;
    }

    bool getSetting(const char *settingName, bool defaultValue) { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue) { return defaultValue; };
    const char *appSpecificLocation(const std::string &name) { return name.c_str(); };

    bool properNameKey = false;
    bool properVersionKey = false;
};

TEST(DriverInfo, GivenDriverInfoWhenThenReturnNonNullptr) {
    MockDriverInfoWindows driverInfo("");
    RegistryReaderMock *registryReaderMock = new RegistryReaderMock();

    driverInfo.registryReader.reset(registryReaderMock);

    std::string defaultName = "defaultName";

    auto name = driverInfo.getDeviceName(defaultName);

    EXPECT_STREQ(defaultName.c_str(), name.c_str());
    EXPECT_TRUE(registryReaderMock->properNameKey);

    std::string defaultVersion = "defaultVersion";

    auto driverVersion = driverInfo.getVersion(defaultVersion);

    EXPECT_STREQ(defaultVersion.c_str(), driverVersion.c_str());
    EXPECT_TRUE(registryReaderMock->properVersionKey);
};

TEST(DriverInfo, givenFullPathToRegistryWhenCreatingDriverInfoWindowsThenTheRegistryPathIsTrimmed) {
    std::string registryPath = "Path\\In\\Registry";
    std::string fullRegistryPath = "\\REGISTRY\\MACHINE\\" + registryPath;
    std::string expectedTrimmedRegistryPath = registryPath;
    MockDriverInfoWindows driverInfo(std::move(fullRegistryPath));

    EXPECT_STREQ(expectedTrimmedRegistryPath.c_str(), driverInfo.path.c_str());
};

TEST(DriverInfo, givenInitializedOsInterfaceWhenCreateDriverInfoThenReturnDriverInfoWindowsNotNullptr) {

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    std::unique_ptr<OSInterface> osInterface(new OSInterface());
    osInterface->get()->setWddm(Wddm::createWddm(nullptr, rootDeviceEnvironment));
    EXPECT_NE(nullptr, osInterface->get()->getWddm());

    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(osInterface.get()));

    EXPECT_NE(nullptr, driverInfo);
};

TEST(DriverInfo, givenNotInitializedOsInterfaceWhenCreateDriverInfoThenReturnDriverInfoWindowsNullptr) {

    std::unique_ptr<OSInterface> osInterface;

    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(osInterface.get()));

    EXPECT_EQ(nullptr, driverInfo);
};

TEST(DriverInfo, givenInitializedOsInterfaceWhenCreateDriverInfoWindowsThenSetRegistryReaderWithExpectRegKey) {
    std::string path = "";
    std::unique_ptr<MockDriverInfoWindows> driverInfo(MockDriverInfoWindows::create(path));
    std::unique_ptr<TestedRegistryReader> reader(new TestedRegistryReader(path));
    EXPECT_NE(nullptr, reader);
    EXPECT_STREQ(driverInfo->getRegistryReaderRegKey(), reader->getRegKey());
};

} // namespace NEO
