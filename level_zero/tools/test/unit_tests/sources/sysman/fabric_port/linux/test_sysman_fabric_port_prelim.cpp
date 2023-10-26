/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"
#include "level_zero/tools/source/sysman/fabric_port/linux/fabric_device_access_imp.h"
#include "level_zero/tools/source/sysman/fabric_port/linux/os_fabric_port_imp_prelim.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/fabric_port/linux/mock_fabric_device.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/nl_api/mock_iaf_nl_api_prelim.h"

#include "gtest/gtest.h"

#include <limits>

extern bool sysmanUltsEnable;
namespace L0 {
namespace ult {

constexpr auto maxNumPorts = 32u;

class PublicFabricDeviceAccessNl : public FabricDeviceAccessNl {
  public:
    using FabricDeviceAccessNl::fabricPorts;
    using FabricDeviceAccessNl::pIafNlApi;
};

class PublicLinuxFabricDeviceImp : public LinuxFabricDeviceImp {
  public:
    using LinuxFabricDeviceImp::pFabricDeviceAccess;
};

class PublicFabricDeviceImp : public FabricDeviceImp {
  public:
    using FabricDeviceImp::pOsFabricDevice;
};

class ZesFabricPortFixture : public SysmanDeviceFixture {
  protected:
    MockIafNlApi *pMockIafNlApi = nullptr;
    FabricPortHandleContext *pFabricPortHandleContext = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pFabricPortHandleContext = pSysmanDeviceImp->pFabricPortHandleContext;
        if (nullptr != pFabricPortHandleContext->pFabricDevice) {
            pFabricPortHandleContext->handleList.clear();
            delete pFabricPortHandleContext->pFabricDevice;
            pFabricPortHandleContext->pFabricDevice = nullptr;
        }
        pFabricPortHandleContext->pFabricDevice = new FabricDeviceImp(pOsSysman);

        PublicLinuxFabricDeviceImp *pPublicLinuxFabricDeviceImp = reinterpret_cast<PublicLinuxFabricDeviceImp *>(pFabricPortHandleContext->pFabricDevice->getOsFabricDevice());
        PublicFabricDeviceAccessNl *pPublicFabricDeviceAccessNl = reinterpret_cast<PublicFabricDeviceAccessNl *>(pPublicLinuxFabricDeviceImp->pFabricDeviceAccess);
        delete pPublicFabricDeviceAccessNl->pIafNlApi;
        pMockIafNlApi = new MockIafNlApi;
        pPublicFabricDeviceAccessNl->pIafNlApi = pMockIafNlApi;
        pPublicFabricDeviceAccessNl->fabricPorts.clear();
        setupDefaults();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }

    void setupDefaults();
};

void ZesFabricPortFixture::setupDefaults() {
    pMockIafNlApi->validateParams = false;
}

TEST_F(ZesFabricPortFixture, GivenFabricDeviceImpValidatepOsFabricDevice) {
    PublicFabricDeviceImp *pPublicFabricDeviceImp = reinterpret_cast<PublicFabricDeviceImp *>(pFabricPortHandleContext->pFabricDevice);
    PublicLinuxFabricDeviceImp *pPublicLinuxFabricDeviceImp = reinterpret_cast<PublicLinuxFabricDeviceImp *>(pPublicFabricDeviceImp->pOsFabricDevice);
    EXPECT_EQ(pFabricPortHandleContext->pFabricDevice->getOsFabricDevice(), pPublicLinuxFabricDeviceImp);
}

TEST_F(ZesFabricPortFixture, GivenPortCountZeroAndValidHandlePtrWhenCallingZesFabricPortGetThenCountIsReturnedAndNoHandlesReturnedAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFabricPorts(device, &count, NULL));
    if (nullptr != pFabricPortHandleContext->pFabricDevice) {
        pFabricPortHandleContext->handleList.clear();
    }

    pMockIafNlApi->numPorts = 0U;
    count = pMockIafNlApi->numPorts;
    zes_fabric_port_handle_t handle = static_cast<zes_fabric_port_handle_t>(0UL);

    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, &handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);
    EXPECT_EQ(handle, static_cast<zes_fabric_port_handle_t>(0UL));
}

TEST_F(ZesFabricPortFixture, GivenPortCountCorrectWhenCallingZesFabricPortGetThenCountHandlesAreReturnedAndAndVerifyZesFabricPortGetCallSucceeds) {
    pMockIafNlApi->numPorts = 1U;
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);
}

TEST_F(ZesFabricPortFixture, GivenPortCountGreaterThanPortsWhenCallingZesFabricPortGetThenCorrectCountIsReturnedAndAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts + 1U;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);
}

TEST_F(ZesFabricPortFixture, GivenPortCounLessThanPortsWhenCallingZesFabricPortGetThenCountLessThenPortsHandlesAreReturned) {
    uint32_t count = pMockIafNlApi->numPorts - 1U;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts - 1U);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetPropertiesThenZesFabricPortGetPropertiesCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    zes_fabric_port_properties_t properties;
    // Initialize values
    properties.onSubdevice = true;
    properties.subdeviceId = std::numeric_limits<uint32_t>::max();
    std::memset(properties.model, std::numeric_limits<int8_t>::max(), ZES_MAX_FABRIC_PORT_MODEL_SIZE);
    properties.portId.fabricId = std::numeric_limits<uint32_t>::max();
    properties.portId.attachId = std::numeric_limits<uint32_t>::max();
    properties.portId.portNumber = std::numeric_limits<uint8_t>::max();
    properties.maxRxSpeed.bitRate = std::numeric_limits<int64_t>::max();
    properties.maxRxSpeed.width = std::numeric_limits<int32_t>::max();
    properties.maxTxSpeed.bitRate = std::numeric_limits<int64_t>::max();
    properties.maxTxSpeed.width = std::numeric_limits<int32_t>::max();

    uint8_t portNumberIndex = pMockIafNlApi->testPortId.portNumber - 1U;

    if (1U == pMockIafNlApi->testPortId.attachId) {
        portNumberIndex += pMockIafNlApi->portsPerSubdevice;
    }

    result = zesFabricPortGetProperties(hPorts[portNumberIndex], &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(pMockIafNlApi->testPortId.attachId, properties.subdeviceId);
    EXPECT_STREQ("XeLink", properties.model);
    EXPECT_EQ(pMockIafNlApi->testPortId.fabricId, properties.portId.fabricId);
    EXPECT_EQ(pMockIafNlApi->testPortId.attachId, properties.portId.attachId);
    EXPECT_EQ(pMockIafNlApi->testPortId.portNumber, properties.portId.portNumber);
    EXPECT_EQ(-1L, properties.maxRxSpeed.bitRate);
    EXPECT_EQ(-1, properties.maxRxSpeed.width);
    EXPECT_EQ(-1L, properties.maxTxSpeed.bitRate);
    EXPECT_EQ(-1, properties.maxTxSpeed.width);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetLinkTypeThenZesFabricPortGetLinkTypeCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    for (auto i = 0U; i < count; i++) {
        zes_fabric_link_type_t linkType;
        result = zesFabricPortGetLinkType(hPorts[i], &linkType);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_STREQ("XeLink", linkType.desc);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetConfigThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    for (auto i = 0U; i < count; i++) {
        pMockIafNlApi->portEnabled = false;
        pMockIafNlApi->portBeaconing = false;

        zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};

        result = zesFabricPortGetConfig(hPorts[i], &getConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(getConfig.enabled);
        EXPECT_FALSE(getConfig.beaconing);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingTrueThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    for (auto i = 0U; i < count; i++) {
        pMockIafNlApi->portEnabled = false;
        pMockIafNlApi->portBeaconing = false;

        zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};

        result = zesFabricPortSetConfig(hPorts[i], &setConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    for (auto i = 0U; i < count; i++) {
        zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};

        result = zesFabricPortGetConfig(hPorts[i], &getConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(getConfig.enabled);
        EXPECT_TRUE(getConfig.beaconing);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledFalseAndBeaconingFalseThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    for (auto i = 0U; i < count; i++) {
        pMockIafNlApi->portEnabled = true;
        pMockIafNlApi->portBeaconing = true;

        zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};

        result = zesFabricPortSetConfig(hPorts[i], &setConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    for (auto i = 0U; i < count; i++) {
        zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};

        result = zesFabricPortGetConfig(hPorts[i], &getConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(getConfig.enabled);
        EXPECT_FALSE(getConfig.beaconing);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledAlreadyTrueAndBeaconingAlreadyTrueThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    for (auto i = 0U; i < count; i++) {
        pMockIafNlApi->portEnabled = true;
        pMockIafNlApi->portBeaconing = true;

        zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};

        result = zesFabricPortSetConfig(hPorts[i], &setConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    for (auto i = 0U; i < count; i++) {
        zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};

        result = zesFabricPortGetConfig(hPorts[i], &getConfig);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(getConfig.enabled);
        EXPECT_TRUE(getConfig.beaconing);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledAlreadyTrueAndBeaconingAlreadyTrueAndGetPortBeaconStateFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = true;
    pMockIafNlApi->portBeaconing = true;
    pMockIafNlApi->mockportBeaconStateQueryReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingTrueAndEnablePortFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = false;
    pMockIafNlApi->portBeaconing = false;
    pMockIafNlApi->mockportEnableReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledFalseAndBeaconingFalseAndPortUsageDisableFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = true;
    pMockIafNlApi->portBeaconing = true;
    pMockIafNlApi->mockportUsageDisableReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledFalseAndBeaconingFalseAndDisableFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = true;
    pMockIafNlApi->portBeaconing = true;
    pMockIafNlApi->mockportDisableReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledFalseAndBeaconingFalseAndEnableUsageFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = true;
    pMockIafNlApi->portBeaconing = true;
    pMockIafNlApi->mockportUsageEnableReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingFalseAndEnableUsageFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = false;
    pMockIafNlApi->portBeaconing = true;
    pMockIafNlApi->mockportStateQueryReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = false};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingTrueAndForceSweepFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = false;
    pMockIafNlApi->portBeaconing = false;
    pMockIafNlApi->mockremRequestReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingTrueAndFirstRoutingQueryFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = false;
    pMockIafNlApi->portBeaconing = false;
    pMockIafNlApi->mockroutingGenQueryReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pMockIafNlApi->isRepeated = true;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigAndEnabledTrueAndBeaconingTrueAndSecondRoutingQueryFailsThenZesFabricPortGetConfigCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->portEnabled = false;
    pMockIafNlApi->portBeaconing = false;
    pMockIafNlApi->mockroutingGenQueryReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pMockIafNlApi->mockroutingGenQueryReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pMockIafNlApi->isRepeated = true;

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortSetConfig(hPorts[0], &setConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenWhenCallingZesFabricPortGetConfigAndGetPortEnabledStateReturnsUnAvailableThenCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->mockportStateQueryReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortGetConfig(hPorts[0], &getConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenWhenCallingZesFabricPortGetConfigAndGetPortBeaconStateReturnsUnAvailableThenCallFails) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    pMockIafNlApi->mockportBeaconStateQueryReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortGetConfig(hPorts[0], &getConfig);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetStateThenZesFabricPortGetStateCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    uint8_t portNumberIndex = pMockIafNlApi->testPortId.portNumber - 1U;

    if (1U == pMockIafNlApi->testPortId.attachId) {
        portNumberIndex += pMockIafNlApi->portsPerSubdevice;
    }

    zes_fabric_port_state_t state = {};
    state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
    state.pNext = nullptr;
    result = zesFabricPortGetState(hPorts[portNumberIndex], &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, state.stype);
    EXPECT_EQ(nullptr, state.pNext);
    EXPECT_EQ(pMockIafNlApi->fPortStatus, state.status);
    EXPECT_EQ(0U, state.qualityIssues);
    EXPECT_EQ(0U, state.failureReasons);
    EXPECT_EQ(pMockIafNlApi->testPortId.fabricId, state.remotePortId.fabricId);
    EXPECT_EQ(pMockIafNlApi->testPortId.attachId, state.remotePortId.attachId);
    EXPECT_EQ(pMockIafNlApi->testPortId.portNumber + 1, state.remotePortId.portNumber);
    EXPECT_EQ(-1L, state.rxSpeed.bitRate);
    EXPECT_EQ(-1, state.rxSpeed.width);
    EXPECT_EQ(-1L, state.txSpeed.bitRate);
    EXPECT_EQ(-1, state.txSpeed.width);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetThroughputThenZesFabricPortGetThroughputCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, pMockIafNlApi->numPorts);

    uint8_t portNumberIndex = pMockIafNlApi->testPortId.portNumber - 1U;

    if (1U == pMockIafNlApi->testPortId.attachId) {
        portNumberIndex += pMockIafNlApi->portsPerSubdevice;
    }

    zes_fabric_port_throughput_t throughput;
    // Initialize values
    throughput.timestamp = 0LU;
    throughput.rxCounter = std::numeric_limits<uint64_t>::max();
    throughput.txCounter = std::numeric_limits<uint64_t>::max();

    result = zesFabricPortGetThroughput(hPorts[portNumberIndex], &throughput);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // Timestamp is not provided by IafNlApi
    EXPECT_EQ(pMockIafNlApi->rxCounter, throughput.rxCounter);
    EXPECT_EQ(pMockIafNlApi->txCounter, throughput.txCounter);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetMultiPortThroughputThenZesFabricPortGetMultiPortThroughputCallSucceeds) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_fabric_port_throughput_t throughputArray[maxNumPorts] = {};
    zes_fabric_port_throughput_t *pThroughput = throughputArray;
    result = zesFabricPortGetMultiPortThroughput(device, count, hPorts, &pThroughput);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pMockIafNlApi->rxCounter, throughputArray[i].rxCounter);
        EXPECT_EQ(pMockIafNlApi->txCounter, throughputArray[i].txCounter);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingGetMultiPortThroughputAndGetThroughputThenVerifyThroughputFromBothApisAreEqual) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint8_t portNumberIndex = pMockIafNlApi->testPortId.portNumber - 1U;

    if (1U == pMockIafNlApi->testPortId.attachId) {
        portNumberIndex += pMockIafNlApi->portsPerSubdevice;
    }

    zes_fabric_port_throughput_t throughput = {};
    result = zesFabricPortGetThroughput(hPorts[portNumberIndex], &throughput);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_fabric_port_throughput_t throughputArray[1] = {};
    zes_fabric_port_throughput_t *pThroughput = throughputArray;
    result = zesFabricPortGetMultiPortThroughput(device, 1, &hPorts[portNumberIndex], &pThroughput);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(throughput.rxCounter, throughputArray[0].rxCounter);
    EXPECT_EQ(throughput.txCounter, throughputArray[0].txCounter);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetMultiPortThroughputWithCountAsZeroThenApiCallReturnsFailure) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_fabric_port_throughput_t throughputArray[maxNumPorts] = {};
    zes_fabric_port_throughput_t *pThroughput = throughputArray;
    result = zesFabricPortGetMultiPortThroughput(device, 0, hPorts, &pThroughput);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetMultiPortThroughputAndIafGetThroughPutFailsThenApiCallReturnsFailure) {
    uint32_t count = pMockIafNlApi->numPorts;
    ASSERT_LE(count, maxNumPorts);
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pMockIafNlApi->mockgetMultiPortThroughPutReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    zes_fabric_port_throughput_t throughputArray[maxNumPorts] = {};
    zes_fabric_port_throughput_t *pThroughput = throughputArray;
    result = zesFabricPortGetMultiPortThroughput(device, count, hPorts, &pThroughput);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetMultiPortThroughputAndPortIdAreInvalidThenCountersOfZeroAreReturned) {
    uint32_t count = 3;
    zes_fabric_port_handle_t hPorts[maxNumPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    IafPortId invalidPortNumber = {pMockIafNlApi->defaultFabricId, pMockIafNlApi->testPortId.attachId, pMockIafNlApi->testPortId.portNumber + 1U};
    IafPortId invalidAttachId = {pMockIafNlApi->defaultFabricId, pMockIafNlApi->testPortId.attachId + 10U, pMockIafNlApi->testPortId.portNumber};
    IafPortId invalidFabricId = {pMockIafNlApi->defaultFabricId + 20U, pMockIafNlApi->testPortId.attachId, pMockIafNlApi->testPortId.portNumber};
    pMockIafNlApi->mockIafPortIds.insert(pMockIafNlApi->mockIafPortIds.end(), {invalidPortNumber, invalidAttachId, invalidFabricId});

    zes_fabric_port_throughput_t throughputArray[maxNumPorts] = {};
    zes_fabric_port_throughput_t *pThroughput = throughputArray;
    result = zesFabricPortGetMultiPortThroughput(device, count, hPorts, &pThroughput);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(0U, throughputArray[i].rxCounter);
        EXPECT_EQ(0U, throughputArray[i].txCounter);
    }
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersWithLegacyPathAndCallSucceeds) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorPath = "/mockRealPath/iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::vector<std::string> dirs = {"driver", "drm", "iaf.5"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorPath + "/fw_error", mockFwErrors},
        {fabricFwErrorPath + "/fw_comm_errors", mockFwCommErrors},
    };
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, mockLinkFailures);
    EXPECT_EQ(errors.linkDegradeCount, mockLinkDegrades);
    EXPECT_EQ(errors.fwErrorCount, mockFwErrors);
    EXPECT_EQ(errors.fwCommErrorCount, mockFwCommErrors);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersCallSucceeds) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::vector<std::string> dirs = {"i915.iaf.5",
                                     "driver", "drm"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorPath + "/fw_error", mockFwErrors},
        {fabricFwErrorPath + "/fw_comm_errors", mockFwCommErrors},
    };
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, mockLinkFailures);
    EXPECT_EQ(errors.linkDegradeCount, mockLinkDegrades);
    EXPECT_EQ(errors.fwErrorCount, mockFwErrors);
    EXPECT_EQ(errors.fwCommErrorCount, mockFwCommErrors);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndGetRealPathFailsThenFailureIsReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    static_cast<MockFabricSysFsAccess *>(pSysfsAccess)->mockRealPathStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndListDirectoryFailsThenFailureIsReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    static_cast<MockFabricFsAccess *>(pFsAccess)->mockListDirectory = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndIafSysfsFilesAreNotPresentThenFailureIsReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    std::vector<std::string> dirs = {"driver", "drm"};
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);

    zes_fabric_port_error_counters_t errors;
    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndLinkFailureSysfsNodeIsAbsentThenZeroLinkFailuresAreReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorIncorrectPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId + 1) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricLinkErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::vector<std::string> dirs = {"i915.iaf.5",
                                     "driver", "drm"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorIncorrectPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorPath + "/fw_error", mockFwErrors},
        {fabricFwErrorPath + "/fw_comm_errors", mockFwCommErrors},
    };
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, 0u);
    EXPECT_EQ(errors.linkDegradeCount, mockLinkDegrades);
    EXPECT_EQ(errors.fwErrorCount, mockFwErrors);
    EXPECT_EQ(errors.fwCommErrorCount, mockFwCommErrors);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndLinkDegradesSysfsNodeIsAbsentThenZeroLinkDegradesAreReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricLinkErrorIncorrectPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId + 1) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::vector<std::string> dirs = {"i915.iaf.5",
                                     "driver", "drm"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorIncorrectPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorPath + "/fw_error", mockFwErrors},
        {fabricFwErrorPath + "/fw_comm_errors", mockFwCommErrors},
    };

    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, mockLinkFailures);
    EXPECT_EQ(errors.linkDegradeCount, 0u);
    EXPECT_EQ(errors.fwErrorCount, mockFwErrors);
    EXPECT_EQ(errors.fwCommErrorCount, mockFwCommErrors);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndFwErrorSysfsNodeIsAbsentThenZeroFwErrorsAreReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::string fabricFwErrorIncorrectPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId + 1);
    std::vector<std::string> dirs = {"i915.iaf.5",
                                     "driver", "drm"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorIncorrectPath + "/fw_error", mockFwErrors},
        {fabricFwErrorPath + "/fw_comm_errors", mockFwCommErrors},
    };

    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, mockLinkFailures);
    EXPECT_EQ(errors.linkDegradeCount, mockLinkDegrades);
    EXPECT_EQ(errors.fwErrorCount, 0u);
    EXPECT_EQ(errors.fwCommErrorCount, mockFwCommErrors);

    delete pFsAccess;
    delete pSysfsAccess;
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingzesFabricPortGetFabricErrorCountersAndFwCommErrorSysfsNodeIsAbsentThenZeroFwCommErrorsAreReturned) {
    VariableBackup<FsAccess *> backupFsAccess(&pLinuxSysmanImp->pFsAccess);
    VariableBackup<SysfsAccess *> backupSysfsAccess(&pLinuxSysmanImp->pSysfsAccess);
    auto pFsAccess = new MockFabricFsAccess;
    auto pSysfsAccess = new MockFabricSysFsAccess;
    pLinuxSysmanImp->pFsAccess = pFsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;

    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;

    result = zesFabricPortGetProperties(hPorts[0], &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::string fabricLinkErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId) + "/port." + std::to_string(properties.portId.portNumber);
    std::string fabricFwErrorPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId);
    std::string fabricFwErrorIncorrectPath = "/mockRealPath/i915.iaf.5/sd." + std::to_string(properties.portId.attachId + 1);
    std::vector<std::string> dirs = {"i915.iaf.5",
                                     "driver", "drm"};
    uint64_t mockLinkFailures = 401;
    uint64_t mockLinkDegrades = 501;
    uint64_t mockFwErrors = 301;
    uint64_t mockFwCommErrors = 201;
    std::map<std::string, uint64_t> nodes = {
        {fabricLinkErrorPath + "/link_failures", mockLinkFailures},
        {fabricLinkErrorPath + "/link_degrades", mockLinkDegrades},
        {fabricFwErrorPath + "/fw_error", mockFwErrors},
        {fabricFwErrorIncorrectPath + "/fw_comm_errors", mockFwCommErrors},
    };
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleDirectories(dirs);
    static_cast<MockFabricFsAccess *>(pFsAccess)->setAccessibleNodes(nodes);
    zes_fabric_port_error_counters_t errors;

    result = zesFabricPortGetFabricErrorCounters(hPorts[0], &errors);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(errors.linkFailureCount, mockLinkFailures);
    EXPECT_EQ(errors.linkDegradeCount, mockLinkDegrades);
    EXPECT_EQ(errors.fwErrorCount, mockFwErrors);
    EXPECT_EQ(errors.fwCommErrorCount, 0u);

    delete pFsAccess;
    delete pSysfsAccess;
}

} // namespace ult
} // namespace L0
