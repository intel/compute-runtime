/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/windows/driver_info.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/registry_reader.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/registry_reader_tests.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[IGFX_MAX_CORE];

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump, ExecutionEnvironment &executionEnvironment);

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

CommandStreamReceiver *createMockCommandStreamReceiver(bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    auto csr = new MockCommandStreamReceiver(executionEnvironment);
    if (!executionEnvironment.osInterface) {
        executionEnvironment.osInterface = std::make_unique<OSInterface>();
        auto wddm = new WddmMock();
        auto hwInfo = *executionEnvironment.getHardwareInfo();
        wddm->init(hwInfo);
        executionEnvironment.osInterface->get()->setWddm(wddm);
    }

    EXPECT_NE(nullptr, executionEnvironment.osInterface.get());
    csr->setOSInterface(executionEnvironment.osInterface.get());
    return csr;
}

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWhenCorrectOSInterfaceThenCreateDriverInfo) {
    VariableBackup<bool> backup(&overrideCommandStreamReceiverCreation, true);
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo);

    EXPECT_TRUE(device->hasDriverInfo());
    delete device;
}

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWithoutCorrectOSInterfaceThenDontCreateDriverInfo) {
    VariableBackup<bool> backup(&overrideCommandStreamReceiverCreation, false);
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo);

    EXPECT_FALSE(device->hasDriverInfo());
    delete device;
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
    DriverInfoWindows driverInfo;
    RegistryReaderMock *registryReaderMock = new RegistryReaderMock();

    driverInfo.setRegistryReader(registryReaderMock);

    std::string defaultName = "defaultName";

    auto name = driverInfo.getDeviceName(defaultName);

    EXPECT_STREQ(defaultName.c_str(), name.c_str());
    EXPECT_TRUE(registryReaderMock->properNameKey);

    std::string defaultVersion = "defaultVersion";

    auto driverVersion = driverInfo.getVersion(defaultVersion);

    EXPECT_STREQ(defaultVersion.c_str(), driverVersion.c_str());
    EXPECT_TRUE(registryReaderMock->properVersionKey);
};

TEST(DriverInfo, givenInitializedOsInterfaceWhenCreateDriverInfoThenReturnDriverInfoWindowsNotNullptr) {

    std::unique_ptr<OSInterface> osInterface(new OSInterface());
    osInterface->get()->setWddm(Wddm::createWddm());
    EXPECT_NE(nullptr, osInterface->get()->getWddm());

    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(osInterface.get()));

    EXPECT_NE(nullptr, driverInfo);
};

TEST(DriverInfo, givenNotInitializedOsInterfaceWhenCreateDriverInfoThenReturnDriverInfoWindowsNullptr) {

    std::unique_ptr<OSInterface> osInterface;

    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(osInterface.get()));

    EXPECT_EQ(nullptr, driverInfo);
};

class MockDriverInfoWindows : public DriverInfoWindows {

  public:
    const char *getRegistryReaderRegKey() {
        return reader->getRegKey();
    }
    TestedRegistryReader *reader;

    static MockDriverInfoWindows *create(std::string path) {

        auto result = new MockDriverInfoWindows();
        result->reader = new TestedRegistryReader(path);
        result->setRegistryReader(result->reader);

        return result;
    };
};

TEST(DriverInfo, givenInitializedOsInterfaceWhenCreateDriverInfoWindowsThenSetRegistryReaderWithExpectRegKey) {
    std::string path = "";
    std::unique_ptr<MockDriverInfoWindows> driverInfo(MockDriverInfoWindows::create(path));
    std::unique_ptr<TestedRegistryReader> reader(new TestedRegistryReader(path));
    EXPECT_NE(nullptr, reader);
    EXPECT_STREQ(driverInfo->getRegistryReaderRegKey(), reader->getRegKey());
};

} // namespace NEO
