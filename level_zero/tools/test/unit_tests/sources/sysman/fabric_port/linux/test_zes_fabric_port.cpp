/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/fabric_port/linux/mock_fabric_device.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <limits>

extern bool sysmanUltsEnable;

using ::testing::Return;

namespace L0 {
namespace ult {

class ZesFabricPortFixture : public SysmanDeviceFixture {
  protected:
    static uint32_t numPorts;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        FabricPortHandleContext *pFabricPortHandleContext = pSysmanDeviceImp->pFabricPortHandleContext;
        if (nullptr != pFabricPortHandleContext->pFabricDevice) {
            for (FabricPort *pFabricPort : pFabricPortHandleContext->handleList) {
                delete pFabricPort;
            }
            pFabricPortHandleContext->handleList.clear();
            delete pFabricPortHandleContext->pFabricDevice;
            pFabricPortHandleContext->pFabricDevice = nullptr;
        }
        Mock<FabricDevice> *mockFabricDevice = new NiceMock<Mock<FabricDevice>>;
        ON_CALL(*mockFabricDevice, getNumPorts())
            .WillByDefault(Return(numPorts));

        pFabricPortHandleContext->pFabricDevice = mockFabricDevice;
        pFabricPortHandleContext->init();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }
};

uint32_t ZesFabricPortFixture::numPorts = 2U;

TEST_F(ZesFabricPortFixture, GivenPortCountZeroWhenCallingZesFabricPortGetThenCountIsReturnedAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = 0U;
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, ZesFabricPortFixture::numPorts);
}

TEST_F(ZesFabricPortFixture, GivenPortCountZeroAndValidHandlePtrWhenCallingZesFabricPortGetThenCountIsReturnedAndNoHandlesReturnedAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = 0U;
    zes_fabric_port_handle_t handle = static_cast<zes_fabric_port_handle_t>(0UL);

    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, &handle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, ZesFabricPortFixture::numPorts);
    EXPECT_EQ(handle, static_cast<zes_fabric_port_handle_t>(0UL));
}

TEST_F(ZesFabricPortFixture, GivenPortCountCorrectWhenCallingZesFabricPortGetThenCountHandlesAreReturnedAndAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = ZesFabricPortFixture::numPorts;
    zes_fabric_port_handle_t hPorts[ZesFabricPortFixture::numPorts];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, ZesFabricPortFixture::numPorts);
}

TEST_F(ZesFabricPortFixture, GivenPortCountGreaterThanPortsWhenCallingZesFabricPortGetThenCorrectCountisReturnedAndAndVerifyZesFabricPortGetCallSucceeds) {
    uint32_t count = ZesFabricPortFixture::numPorts + 1U;
    zes_fabric_port_handle_t hPorts[ZesFabricPortFixture::numPorts + 1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, ZesFabricPortFixture::numPorts);
}

TEST_F(ZesFabricPortFixture, GivenPortCounLessThanPortsWhenCallingZesFabricPortGetThenCountLessTanPortsHandlesAreReturned) {
    uint32_t count = ZesFabricPortFixture::numPorts - 1U;
    zes_fabric_port_handle_t hPorts[ZesFabricPortFixture::numPorts - 1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, ZesFabricPortFixture::numPorts - 1U);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetPropertiesThenZesFabricPortGetPropertiesCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_properties_t properties;
    // Initialize values
    properties.onSubdevice = true;
    properties.subdeviceId = std::numeric_limits<uint32_t>::max();
    std::memset(properties.model, std::numeric_limits<int8_t>::max(), ZES_MAX_FABRIC_PORT_MODEL_SIZE);
    properties.portId.fabricId = std::numeric_limits<uint32_t>::max();
    properties.portId.attachId = std::numeric_limits<uint32_t>::max();
    properties.portId.portNumber = std::numeric_limits<uint8_t>::max();
    properties.maxRxSpeed.bitRate = std::numeric_limits<uint64_t>::max();
    properties.maxRxSpeed.width = std::numeric_limits<uint32_t>::max();
    properties.maxTxSpeed.bitRate = std::numeric_limits<uint64_t>::max();
    properties.maxTxSpeed.width = std::numeric_limits<uint32_t>::max();

    result = zesFabricPortGetProperties(hPorts[0], &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(0L, properties.subdeviceId);
    EXPECT_STREQ("EXAMPLE", properties.model);
    EXPECT_EQ(0U, properties.portId.fabricId);
    EXPECT_EQ(0U, properties.portId.attachId);
    EXPECT_EQ(0U, properties.portId.portNumber);
    EXPECT_EQ(0L, properties.maxRxSpeed.bitRate);
    EXPECT_EQ(0, properties.maxRxSpeed.width);
    EXPECT_EQ(0L, properties.maxTxSpeed.bitRate);
    EXPECT_EQ(0, properties.maxTxSpeed.width);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetLinkTypeThenZesFabricPortGetLinkTypeCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_link_type_t linkType;
    result = zesFabricPortGetLinkType(hPorts[0], &linkType);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_STREQ("SAMPLE LINK, VERBOSE", linkType.desc);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetConfigThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = true};
    result = zesFabricPortGetConfig(hPorts[0], &getConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(getConfig.enabled);
    EXPECT_FALSE(getConfig.beaconing);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortSetConfigThenZesFabricPortGetConfigCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_config_t setConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = true, .beaconing = false};
    result = zesFabricPortSetConfig(hPorts[0U], &setConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_fabric_port_config_t getConfig = {.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, .pNext = nullptr, .enabled = false, .beaconing = true};
    result = zesFabricPortGetConfig(hPorts[0], &getConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(getConfig.enabled);
    EXPECT_FALSE(getConfig.beaconing);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetStateThenZesFabricPortGetStateCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_state_t state;
    result = zesFabricPortGetState(hPorts[0], &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0U, state.status);
    EXPECT_EQ(0U, state.qualityIssues);
    EXPECT_EQ(0U, state.failureReasons);
    EXPECT_EQ(0U, state.remotePortId.fabricId);
    EXPECT_EQ(0U, state.remotePortId.attachId);
    EXPECT_EQ(0U, state.remotePortId.portNumber);
    EXPECT_EQ(0L, state.rxSpeed.bitRate);
    EXPECT_EQ(0, state.rxSpeed.width);
    EXPECT_EQ(0L, state.txSpeed.bitRate);
    EXPECT_EQ(0, state.txSpeed.width);
}

TEST_F(ZesFabricPortFixture, GivenValidFabricPortHandleWhenCallingZesFabricPortGetThroughputThenZesFabricPortGetThroughputCallSucceeds) {
    uint32_t count = 1U;
    zes_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zesDeviceEnumFabricPorts(device, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zes_fabric_port_throughput_t throughput;
    // Initialize values
    throughput.timestamp = 0LU;
    throughput.rxCounter = std::numeric_limits<uint64_t>::max();
    throughput.txCounter = std::numeric_limits<uint64_t>::max();

    result = zesFabricPortGetThroughput(hPorts[0], &throughput);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0LU, throughput.timestamp);
    EXPECT_EQ(0LU, throughput.rxCounter);
    EXPECT_EQ(0LU, throughput.txCounter);
}

} // namespace ult
} // namespace L0
