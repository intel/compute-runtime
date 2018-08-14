/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/os_interface/windows/driver_info.h"
#include "runtime/os_interface/windows/registry_reader.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/helpers/options.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/libult/create_command_stream.h"
#include "gtest/gtest.h"

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createMockCommandStreamReceiver(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment);

class DriverInfoDeviceTest : public ::testing::Test {
  public:
    void SetUp() {
        hwInfo = platformDevices[0];
        commandStreamReceiverCreateFunc = commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily];
        commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily] = createMockCommandStreamReceiver;
    }

    void TearDown() {
        commandStreamReceiverFactory[hwInfo->pPlatform->eRenderCoreFamily] = commandStreamReceiverCreateFunc;
    }

    CommandStreamReceiverCreateFunc commandStreamReceiverCreateFunc;
    const HardwareInfo *hwInfo;
};

CommandStreamReceiver *createMockCommandStreamReceiver(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    auto csr = new MockCommandStreamReceiver();
    OSInterface *osInterface = new OSInterface();
    executionEnvironment.osInterface.reset(osInterface);
    osInterface->get()->setWddm(new WddmMock());
    csr->setOSInterface(osInterface);
    return csr;
}

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWhenCorrectOSInterfaceThenCreateDriverInfo) {
    overrideCommandStreamReceiverCreation = true;
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo);

    EXPECT_TRUE(device->hasDriverInfo());
    delete device;
}

TEST_F(DriverInfoDeviceTest, GivenDeviceCreatedWithoutCorrectOSInterfaceThenDontCreateDriverInfo) {
    overrideCommandStreamReceiverCreation = false;
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
}
} // namespace OCLRT
