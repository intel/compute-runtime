/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/ras/linux/mock_fs_ras_fabric_prelim.h"

namespace L0 {
namespace ult {
constexpr uint32_t mockHandleCount = 2u;
class TestRasFabricFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockRasFabricFsAccess> pFsAccess;
    std::unique_ptr<MockRasFabricSysFsAccess> pSysfsAccess;
    MemoryManager *pMemoryManagerOriginal = nullptr;
    std::unique_ptr<MockMemoryManagerInRasSysman> pMemoryManager;
    FsAccess *pFsAccessOriginal = nullptr;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    FirmwareUtil *pOriginalFwUtilInterface = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {

        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pMemoryManagerOriginal = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = std::make_unique<::testing::NiceMock<MockMemoryManagerInRasSysman>>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = true;
        device->getDriverHandle()->setMemoryManager(pMemoryManager.get());
        pFsAccess = std::make_unique<MockRasFabricFsAccess>();
        pSysfsAccess = std::make_unique<MockRasFabricSysFsAccess>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pOriginalFwUtilInterface = pLinuxSysmanImp->pFwUtilInterface;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pPmuInterface = nullptr;
        pLinuxSysmanImp->pFwUtilInterface = nullptr;
        for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOriginal);
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFwUtilInterface = pOriginalFwUtilInterface;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(TestRasFabricFixture, GivenValidRasFabricNodesThenGetStateIsSuccessful) {

    std::vector<std::string> dirs = {"/mockRealPath/iaf.0",
                                     "/sys/module/iaf/drivers/platform:iaf/"};
    std::map<std::string, uint64_t> nodes = {
        {"/mockRealPath/iaf.0/sd.0/fw_comm_errors", 101},
        {"/mockRealPath/iaf.0/sd.0/sd_failure", 201},
        {"/mockRealPath/iaf.0/sd.0/fw_error", 301},
        {"/mockRealPath/iaf.0/sd.0/port.1/link_failures", 401},
        {"/mockRealPath/iaf.0/sd.0/port.1/link_degrades", 501},
        {"/mockRealPath/iaf.0/sd.0/port.2/link_failures", 601},
        {"/mockRealPath/iaf.0/sd.0/port.2/link_degrades", 701},
        {"/mockRealPath/iaf.0/sd.0/port.3/link_failures", 801},
        {"/mockRealPath/iaf.0/sd.0/port.3/link_degrades", 901},
        {"/mockRealPath/iaf.0/sd.0/port.4/link_failures", 1001},
        {"/mockRealPath/iaf.0/sd.0/port.4/link_degrades", 1101},
        {"/mockRealPath/iaf.0/sd.0/port.5/link_failures", 2101},
        {"/mockRealPath/iaf.0/sd.0/port.5/link_degrades", 3101},
        {"/mockRealPath/iaf.0/sd.0/port.6/link_failures", 4101},
        {"/mockRealPath/iaf.0/sd.0/port.6/link_degrades", 5101},
        {"/mockRealPath/iaf.0/sd.0/port.7/link_failures", 6101},
        {"/mockRealPath/iaf.0/sd.0/port.7/link_degrades", 7101},
        {"/mockRealPath/iaf.0/sd.0/port.8/link_failures", 8101},
        {"/mockRealPath/iaf.0/sd.0/port.8/link_degrades", 9101},
    };
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleDirectories(dirs);
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodes);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));

        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);

        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 27709u);
        }
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 23710u);
        }
    }
}

TEST_F(TestRasFabricFixture, GivenInValidRasFabricNodesThenEnumerationDoesNotReturnAnyHandles) {

    pSysfsAccess->mockRealPathStatus = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(TestRasFabricFixture, GivenValidRasFabricAuxiliaryNodesThenGetStateIsSuccessful) {

    std::vector<std::string> dirs = {"/mockRealPath/i915.iaf.0",
                                     "/sys/module/iaf/drivers/auxiliary:iaf/"};
    std::map<std::string, uint64_t> nodes = {
        {"/mockRealPath/i915.iaf.0/sd.0/fw_comm_errors", 101},
        {"/mockRealPath/i915.iaf.0/sd.0/sd_failure", 201},
        {"/mockRealPath/i915.iaf.0/sd.0/fw_error", 301},
        {"/mockRealPath/i915.iaf.0/sd.0/port.1/link_failures", 401},
        {"/mockRealPath/i915.iaf.0/sd.0/port.1/link_degrades", 501},
    };
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleDirectories(dirs);
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodes);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));

        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);

        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 602u);
        }
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 903u);
        }
    }
}

TEST_F(TestRasFabricFixture, GivenSomeRasFabricNodesThenGetStateIsSuccessful) {

    std::vector<std::string> dirs = {"/mockRealPath/iaf.31",
                                     "/sys/module/iaf/drivers/platform:iaf/"};
    std::map<std::string, uint64_t> nodes = {
        {"/mockRealPath/iaf.31/sd.0/fw_comm_errors", 101},
        {"/mockRealPath/iaf.31/sd.0/sd_failure", 201},
        {"/mockRealPath/iaf.31/sd.0/fw_error", 301},
        {"/mockRealPath/iaf.31/sd.0/port.1/link_failures", 401},
        {"/mockRealPath/iaf.31/sd.0/port.2/link_failures", 601},
        {"/mockRealPath/iaf.31/sd.0/port.2/link_degrades", 701},
        {"/mockRealPath/iaf.31/sd.0/port.3/link_failures", 801},
        {"/mockRealPath/iaf.31/sd.0/port.3/link_degrades", 901},
        {"/mockRealPath/iaf.31/sd.0/port.4/link_failures", 1001},
        {"/mockRealPath/iaf.31/sd.0/port.4/link_degrades", 1101},
        {"/mockRealPath/iaf.31/sd.0/port.5/link_failures", 2101},
        {"/mockRealPath/iaf.31/sd.0/port.5/link_degrades", 3101},
        {"/mockRealPath/iaf.31/sd.0/port.6/link_failures", 4101},
        {"/mockRealPath/iaf.31/sd.0/port.6/link_degrades", 5101},
        {"/mockRealPath/iaf.31/sd.0/port.7/link_failures", 6101},
        {"/mockRealPath/iaf.31/sd.0/port.7/link_degrades", 7101},
        {"/mockRealPath/iaf.31/sd.0/port.8/link_degrades", 9101},
    };
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleDirectories(dirs);
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodes);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));

        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);

        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 27709u - 501u);
        }
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 23710u - 8101u);
        }
    }
}

TEST_F(TestRasFabricFixture, GivenValidRasFabricNodesWhenGetStateIsCalledTwiceThenRasErrorCountIsDoubled) {

    std::vector<std::string> dirs = {"/mockRealPath/iaf.27",
                                     "/sys/module/iaf/drivers/platform:iaf/"};
    std::map<std::string, uint64_t> nodes = {
        {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 101},
        {"/mockRealPath/iaf.27/sd.0/sd_failure", 201},
        {"/mockRealPath/iaf.27/sd.0/fw_error", 301},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 401},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 501},
    };
    std::map<std::string, uint64_t> nodesSecondRead = {
        {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 101 * 2},
        {"/mockRealPath/iaf.27/sd.0/sd_failure", 201 * 2},
        {"/mockRealPath/iaf.27/sd.0/fw_error", 301 * 2},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 401 * 2},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 501 * 2},
    };
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleDirectories(dirs);
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodes);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
    }
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodesSecondRead);

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);

        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 602u * 2);
        }
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 903u * 2);
        }
    }
}

TEST_F(TestRasFabricFixture, GivenValidRasFabricNodesWhenGetStateIsCalledTwiceWithClearThenNewRasErrorCountIsRetrieved) {

    std::vector<std::string> dirs = {"/mockRealPath/iaf.27",
                                     "/sys/module/iaf/drivers/platform:iaf/"};
    std::map<std::string, uint64_t> nodes = {
        {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 101},
        {"/mockRealPath/iaf.27/sd.0/sd_failure", 201},
        {"/mockRealPath/iaf.27/sd.0/fw_error", 301},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 401},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 501},
    };
    std::map<std::string, uint64_t> nodesSecondRead = {
        {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 101 * 2},
        {"/mockRealPath/iaf.27/sd.0/sd_failure", 201 * 2},
        {"/mockRealPath/iaf.27/sd.0/fw_error", 301 * 2},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 401 * 2},
        {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 501 * 2},
    };
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleDirectories(dirs);
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodes);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 1, &state));
    }
    static_cast<MockRasFabricFsAccess *>(pFsAccess.get())->setAccessibleNodes(nodesSecondRead);

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_ras_state_t state = {};
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);

        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 602u);
        }
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 903u);
        }
    }
}

class SysmanRasFabricMultiDeviceFixture : public MultiDeviceFixture, public ::testing::Test {
  public:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        MultiDeviceFixture::setUp();
        for (auto &device : driverHandle->devices) {
            auto neoDevice = device->getNEODevice();
            neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
            auto &osInterface = device->getOsInterface();
            osInterface.setDriverModel(std::make_unique<SysmanMockDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment())));
            setenv("ZES_ENABLE_SYSMAN", "1", 1);
            delete device->getSysmanHandle();
            device->setSysmanHandle(new SysmanDeviceImp(device->toHandle()));
            auto pSysmanDevice = device->getSysmanHandle();
            for (auto &subDevice : static_cast<DeviceImp *>(device)->subDevices) {
                static_cast<DeviceImp *>(subDevice)->setSysmanHandle(pSysmanDevice);
            }

            auto pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
            auto pOsSysman = pSysmanDeviceImp->pOsSysman;
            auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

            pSysmanDeviceImp->init();

            delete pLinuxSysmanImp->pFwUtilInterface;
            delete pLinuxSysmanImp->pSysfsAccess;
            delete pLinuxSysmanImp->pProcfsAccess;
            delete pLinuxSysmanImp->pFsAccess;

            auto pProcfsAccess = new NiceMock<Mock<LinuxProcfsAccess>>();
            auto pFsAccess = new MockRasFabricFsAccess();
            auto pSysfsAccess = new MockRasFabricSysFsAccess();

            pLinuxSysmanImp->pFwUtilInterface = nullptr;
            pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
            pLinuxSysmanImp->pProcfsAccess = pProcfsAccess;
            pLinuxSysmanImp->pFsAccess = pFsAccess;
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }

        for (auto &device : driverHandle->devices) {
            auto pSysmanDevice = device->getSysmanHandle();
            auto pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
            auto pOsSysman = pSysmanDeviceImp->pOsSysman;
            auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

            delete pLinuxSysmanImp->pSysfsAccess;
            delete pLinuxSysmanImp->pProcfsAccess;
            delete pLinuxSysmanImp->pFsAccess;

            pLinuxSysmanImp->pFwUtilInterface = nullptr;
            pLinuxSysmanImp->pSysfsAccess = nullptr;
            pLinuxSysmanImp->pProcfsAccess = nullptr;
            pLinuxSysmanImp->pFsAccess = nullptr;

            delete pSysmanDevice;
            device->setSysmanHandle(nullptr);
        }

        unsetenv("ZES_ENABLE_SYSMAN");
        MultiDeviceFixture::tearDown();
    }
};

TEST_F(SysmanRasFabricMultiDeviceFixture, GivenValidRasFabricNodesForMultipleDevicesThenGetStateReturnsErrorCountSpecificToEachOfDevice) {

    const uint32_t testUseSubDeviceCount = 2u;
    ASSERT_GE(numRootDevices, 2u);
    ASSERT_GE(numSubDevices, testUseSubDeviceCount);

    std::vector<std::string> dirs = {"/mockRealPath/iaf.27",
                                     "/sys/module/iaf/drivers/platform:iaf/"};
    {
        std::map<std::string, uint64_t> nodes = {
            {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 1},
            {"/mockRealPath/iaf.27/sd.0/sd_failure", 1},
            {"/mockRealPath/iaf.27/sd.0/fw_error", 1},
            {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 1},
            {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 1},

            {"/mockRealPath/iaf.27/sd.1/fw_comm_errors", 2},
            {"/mockRealPath/iaf.27/sd.1/sd_failure", 2},
            {"/mockRealPath/iaf.27/sd.1/fw_error", 2},
            {"/mockRealPath/iaf.27/sd.1/port.1/link_failures", 2},
            {"/mockRealPath/iaf.27/sd.1/port.1/link_degrades", 2},
        };

        auto pOsSysman = static_cast<SysmanDeviceImp *>(driverHandle->devices[0]->getSysmanHandle())->pOsSysman;
        auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        static_cast<MockRasFabricFsAccess *>(pLinuxSysmanImp->pFsAccess)->setAccessibleDirectories(dirs);
        static_cast<MockRasFabricFsAccess *>(pLinuxSysmanImp->pFsAccess)->setAccessibleNodes(nodes);
    }

    {
        std::map<std::string, uint64_t> nodes = {
            {"/mockRealPath/iaf.27/sd.0/fw_comm_errors", 3},
            {"/mockRealPath/iaf.27/sd.0/sd_failure", 3},
            {"/mockRealPath/iaf.27/sd.0/fw_error", 3},
            {"/mockRealPath/iaf.27/sd.0/port.1/link_failures", 3},
            {"/mockRealPath/iaf.27/sd.0/port.1/link_degrades", 3},

            {"/mockRealPath/iaf.27/sd.1/fw_comm_errors", 4},
            {"/mockRealPath/iaf.27/sd.1/sd_failure", 4},
            {"/mockRealPath/iaf.27/sd.1/fw_error", 4},
            {"/mockRealPath/iaf.27/sd.1/port.1/link_failures", 4},
            {"/mockRealPath/iaf.27/sd.1/port.1/link_degrades", 4},
        };

        auto pOsSysman = static_cast<SysmanDeviceImp *>(driverHandle->devices[1]->getSysmanHandle())->pOsSysman;
        auto pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);

        static_cast<MockRasFabricFsAccess *>(pLinuxSysmanImp->pFsAccess)->setAccessibleDirectories(dirs);
        static_cast<MockRasFabricFsAccess *>(pLinuxSysmanImp->pFsAccess)->setAccessibleNodes(nodes);
    }

    const std::vector<std::pair<uint32_t, uint32_t>> errorCounts{
        {2, 3},  // Device 0, subdevice 0
        {4, 6},  // Device 0, subdevice 1
        {6, 9},  // Device 1, subdevice 0
        {8, 12}, // Device 1, subdevice 1
    };

    for (uint32_t deviceIndex = 0; deviceIndex < testUseSubDeviceCount; deviceIndex++) {
        uint32_t count = 0;
        auto hDevice = driverHandle->devices[deviceIndex]->toHandle();
        EXPECT_EQ(zesDeviceEnumRasErrorSets(hDevice, &count, NULL), ZE_RESULT_SUCCESS);
        EXPECT_GT(count, 0u);
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(hDevice, &count, handles.data()), ZE_RESULT_SUCCESS);
        for (auto handle : handles) {
            zes_ras_state_t state = {};
            zes_ras_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));

            const auto accessIndex = deviceIndex * testUseSubDeviceCount + properties.subdeviceId;
            if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
                EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], errorCounts[accessIndex].first);
            }
            if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], errorCounts[accessIndex].second);
            }
        }
    }
}

} // namespace ult
} // namespace L0
