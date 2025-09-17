/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanDeviceFixtureWithCore : public SysmanDeviceFixture, public L0::ult::DeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        L0::ult::DeviceFixture::setUp();
    }
    void TearDown() override {
        L0::ult::DeviceFixture::tearDown();
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanDeviceThenNonNullHandleIsReturned, IsPVC) {
    auto coreDeviceHandle = L0::Device::fromHandle(device->toHandle());
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenNullCoreHandleWhenSysmanHandleIsQueriedThenNullPtrIsReturned, IsPVC) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(nullptr);
    EXPECT_EQ(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenSysmanHandleIsQueriedAndNotFoundInLookUpThenNullPtrIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
        return buf;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
        std::memcpy(buf, str.c_str(), str.size());
        return static_cast<int>(str.size());
    });

    class MockSysmanDriverHandleImp : public SysmanDriverHandleImp {
      public:
        MockSysmanDriverHandleImp() {
            SysmanDriverHandleImp();
        }
        void clearUuidDeviceMap() {
            uuidDeviceMap.clear();
        }
    };
    std::unique_ptr<MockSysmanDriverHandleImp> pSysmanDriverHandleImp = std::make_unique<MockSysmanDriverHandleImp>();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanDriverHandleImp->initialize(*(L0::Sysman::ult::SysmanDeviceFixture::execEnv)));
    pSysmanDriverHandleImp->clearUuidDeviceMap();
    auto pSysmanDevice = pSysmanDriverHandleImp->getSysmanDeviceFromCoreDeviceHandle(L0::Device::fromHandle(device->toHandle()));
    EXPECT_EQ(pSysmanDevice, nullptr);
}

HWTEST2_F(SysmanDeviceFixtureWithCore, GivenValidCoreHandleWhenRetrievingSysmanHandleTwiceThenSuccessAreReturned, IsDG2) {
    auto coreDeviceHandle = L0::Device::fromHandle(device->toHandle());
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
    pSysmanDevice = nullptr;
    pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(coreDeviceHandle);
    EXPECT_NE(pSysmanDevice, nullptr);
}

class MockDriverHandleImpForFromHandle : public DriverHandleImp {
  public:
    enum Mode {
        successTwoCalls,
        failCountQuery,
        zeroDevices,
        failSecondCall
    } mode = successTwoCalls;
    L0::Device *realDevice = nullptr;

    ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) override {
        switch (mode) {
        case failCountQuery:
            return ZE_RESULT_ERROR_UNKNOWN;
        case zeroDevices:
            if (phDevices == nullptr)
                *pCount = 0;
            return ZE_RESULT_SUCCESS;
        case failSecondCall:
            if (phDevices != nullptr) {
                *pCount = 1;
                return ZE_RESULT_SUCCESS;
            }
            return ZE_RESULT_ERROR_UNKNOWN;
        case successTwoCalls:
        default:
            if (phDevices == nullptr) {
                *pCount = 1;
                return ZE_RESULT_SUCCESS;
            }
            DEBUG_BREAK_IF(realDevice == nullptr);
            phDevices[0] = realDevice->toHandle();
            return ZE_RESULT_SUCCESS;
        }
    }
    ze_result_t createContext(const ze_context_desc_t *, uint32_t, ze_device_handle_t *, ze_context_handle_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getProperties(ze_driver_properties_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getApiVersion(ze_api_version_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getIPCProperties(ze_driver_ipc_properties_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getExtensionProperties(uint32_t *, ze_driver_extension_properties_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getExtensionFunctionAddress(const char *, void **) override { return ZE_RESULT_SUCCESS; }
    NEO::MemoryManager *getMemoryManager() override { return nullptr; }
    void setMemoryManager(NEO::MemoryManager *) override {}
};
class MockSysmanDriverHandleForFromHandle : public SysmanDriverHandleImp {
  public:
    bool mockDeviceByUuidSuccess = false;

    ze_result_t getDeviceByUuid(zes_uuid_t uuid, zes_device_handle_t *phDevice, ze_bool_t *onSubdevice, uint32_t *subdeviceId) override {
        if (mockDeviceByUuidSuccess) {
            *phDevice = reinterpret_cast<zes_device_handle_t>(0x2000);
            *onSubdevice = false;
            *subdeviceId = 0;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    void setCoreToSysmanDriverMapping(ze_driver_handle_t coreHandle, SysmanDriverHandle *sysmanHandle) {
        const std::lock_guard<std::mutex> lock(this->coreToSysmanDriverMapLock);
        this->coreToSysmanDriverMap[coreHandle] = sysmanHandle;
    }
    void clearCoreToSysmanDriverMapping() {
        const std::lock_guard<std::mutex> lock(this->coreToSysmanDriverMapLock);
        this->coreToSysmanDriverMap.clear();
    }
};

class SysmanDriverFromHandleFixture : public SysmanDeviceFixtureWithCore {
  protected:
    void SetUp() override {
        SysmanDeviceFixtureWithCore::SetUp();

        originalGlobalDriver = L0::Sysman::globalSysmanDriver;
        mockSysmanDriver = std::make_unique<MockSysmanDriverHandleForFromHandle>();
        mockDriverHandle = std::make_unique<MockDriverHandleImpForFromHandle>();
        L0::Sysman::globalSysmanDriver = mockSysmanDriver.get();
        mockDriverHandle->realDevice = device;
        coreDriverHandle = device->getDriverHandle();
    }

    void TearDown() override {
        L0::Sysman::globalSysmanDriver = originalGlobalDriver;
        SysmanDeviceFixtureWithCore::TearDown();
    }

    std::unique_ptr<MockSysmanDriverHandleForFromHandle> mockSysmanDriver;
    std::unique_ptr<MockDriverHandleImpForFromHandle> mockDriverHandle;
    ze_driver_handle_t coreDriverHandle = nullptr;
    SysmanDriverHandleImp *originalGlobalDriver = nullptr;
};

static void prepareGlobalSysman(MockSysmanDriverHandleForFromHandle *mockSysman) {
    L0::Sysman::driverCount = 1;
    L0::Sysman::globalSysmanDriverHandle = static_cast<_ze_driver_handle_t *>(mockSysman);
}

HWTEST2_F(SysmanDriverFromHandleFixture, GivenNullHandleWhenFromHandleCalledThenReturnsNull, IsPVC) {
    auto result = SysmanDriverHandle::fromHandle(nullptr);
    EXPECT_EQ(result, nullptr);
}

HWTEST2_F(SysmanDriverFromHandleFixture, GivenGlobalSysmanDriverHandleWhenFromHandleCalledThenReturnsGlobalDriver, IsPVC) {
    zes_driver_handle_t globalHandle = static_cast<zes_driver_handle_t>(L0::Sysman::globalSysmanDriver);
    auto result = SysmanDriverHandle::fromHandle(globalHandle);
    EXPECT_EQ(result, L0::Sysman::globalSysmanDriver);
}

HWTEST2_F(SysmanDriverFromHandleFixture, GivenCoreDriverHandleWhenFromHandleCalledAndMappingIsFoundIncoreToSysmanDriverMapThenSysmanDriverIsReturned, IsPVC) {
    mockSysmanDriver->setCoreToSysmanDriverMapping(coreDriverHandle, mockSysmanDriver.get());
    auto result = SysmanDriverHandle::fromHandle(coreDriverHandle);
    mockSysmanDriver->clearCoreToSysmanDriverMapping();
    EXPECT_EQ(result, mockSysmanDriver.get());
}

HWTEST2_F(SysmanDriverFromHandleFixture, GivenCoreDriverHandleWhenFromHandleCalledAndGetDeviceFailsThenNullPtrIsReturned, IsPVC) {
    mockDriverHandle->mode = MockDriverHandleImpForFromHandle::failCountQuery;
    auto coreHandle = static_cast<ze_driver_handle_t>(static_cast<DriverHandle *>(mockDriverHandle.get()));
    prepareGlobalSysman(mockSysmanDriver.get());
    auto result = SysmanDriverHandle::fromHandle(coreHandle);
    mockDriverHandle->mode = MockDriverHandleImpForFromHandle::successTwoCalls;
    EXPECT_EQ(result, nullptr);
}

HWTEST2_F(SysmanDriverFromHandleFixture, GivenCoreDriverHandleWhenFromHandleCalledAndGetDeviceReturnsDeviceCountZeroThenNullPtrIsReturned, IsPVC) {
    mockDriverHandle->mode = MockDriverHandleImpForFromHandle::zeroDevices;
    auto coreHandle = static_cast<ze_driver_handle_t>(static_cast<DriverHandle *>(mockDriverHandle.get()));
    prepareGlobalSysman(mockSysmanDriver.get());
    mockSysmanDriver->clearCoreToSysmanDriverMapping();
    auto result = SysmanDriverHandle::fromHandle(coreHandle);
    mockDriverHandle->mode = MockDriverHandleImpForFromHandle::successTwoCalls;
    EXPECT_EQ(result, nullptr);
}

HWTEST2_F(SysmanDriverFromHandleFixture,
          GivenCoreDriverHandleWhenSysmanDriverCountQueryFailsThenNullIsReturned, IsPVC) {
    mockSysmanDriver->mockDeviceByUuidSuccess = false;
    VariableBackup<uint32_t> backupDriverCount(&L0::Sysman::driverCount, 0u);
    // driverHandleGet will now fail (ZE_RESULT_ERROR_UNINITIALIZED)
    auto result = SysmanDriverHandle::fromHandle(coreDriverHandle);
    EXPECT_EQ(result, nullptr);
    mockSysmanDriver->clearCoreToSysmanDriverMapping();
}

HWTEST2_F(SysmanDriverFromHandleFixture,
          GivenCoreDriverHandleWhenGettingSysmanDriverHandleAndGetDeviceByUuidFailsThenNullPtrIsReturned, IsPVC) {
    VariableBackup<uint32_t> backupDriverCount(&L0::Sysman::driverCount, 1u);
    VariableBackup<_ze_driver_handle_t *> backupGlobal(&L0::Sysman::globalSysmanDriverHandle,
                                                       static_cast<_ze_driver_handle_t *>(L0::Sysman::globalSysmanDriver));
    mockSysmanDriver->mockDeviceByUuidSuccess = false;

    auto result = SysmanDriverHandle::fromHandle(coreDriverHandle);
    EXPECT_EQ(result, nullptr);
    mockSysmanDriver->clearCoreToSysmanDriverMapping();
}

HWTEST2_F(SysmanDriverFromHandleFixture,
          GivenValidCoreDriverHandleWhenGettingSysmanDriverHandleThenValidSysmanHandleIsReturned, IsPVC) {
    VariableBackup<uint32_t> backupDriverCount(&L0::Sysman::driverCount, 1u);
    VariableBackup<_ze_driver_handle_t *> backupGlobal(
        &L0::Sysman::globalSysmanDriverHandle,
        static_cast<_ze_driver_handle_t *>(static_cast<SysmanDriverHandle *>(mockSysmanDriver.get())->toHandle()));

    mockSysmanDriver->mockDeviceByUuidSuccess = true;

    auto result = SysmanDriverHandle::fromHandle(coreDriverHandle);

    EXPECT_EQ(result, static_cast<SysmanDriverHandle *>(mockSysmanDriver.get()));
}

HWTEST2_F(SysmanDriverFromHandleFixture,
          GivenValidCoreDriverHandleWhenGettingSysmanDriverHandleTwiceThenValidSysmanHandleIsReturned, IsPVC) {
    VariableBackup<uint32_t> backupDriverCount(&L0::Sysman::driverCount, 1u);
    VariableBackup<_ze_driver_handle_t *> backupGlobal(
        &L0::Sysman::globalSysmanDriverHandle,
        static_cast<_ze_driver_handle_t *>(static_cast<SysmanDriverHandle *>(mockSysmanDriver.get())->toHandle()));

    mockSysmanDriver->mockDeviceByUuidSuccess = true;

    auto result = SysmanDriverHandle::fromHandle(coreDriverHandle);

    EXPECT_EQ(result, static_cast<SysmanDriverHandle *>(mockSysmanDriver.get()));
    auto result2 = SysmanDriverHandle::fromHandle(coreDriverHandle);
    EXPECT_EQ(result2, static_cast<SysmanDriverHandle *>(mockSysmanDriver.get()));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
