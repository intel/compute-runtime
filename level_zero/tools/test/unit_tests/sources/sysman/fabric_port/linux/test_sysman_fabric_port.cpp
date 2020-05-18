/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/fabric_port/linux/os_fabric_port_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sysman/fabric_port/fabric_port_imp.h"
#include "sysman/linux/os_sysman_imp.h"

#include <limits>

using ::testing::_;

namespace L0 {
namespace ult {

class SysmanFabricPortFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    static uint32_t numPorts;

    void SetUp() override {

        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        hSysman = sysmanImp->toHandle();
        sysmanImp->pFabricPortHandleContext->init();

        for (uint32_t portNum = 0U; portNum < numPorts; portNum++) {
            FabricPort *pFabricPort = new FabricPortImp(sysmanImp->pOsSysman, portNum);
            EXPECT_NE(nullptr, pFabricPort);
            sysmanImp->pFabricPortHandleContext->handleList.push_back(pFabricPort);
        }
    }
    void TearDown() override {
        // Don't free FabricPorts. The FabricPortHandleContext destructor will free them.
        DeviceFixture::TearDown();
    }
};

uint32_t SysmanFabricPortFixture::numPorts = 2U;

TEST_F(SysmanFabricPortFixture, GivenPortCountZeroWhenCallingZetSysmanFabricPortGetThenCountIsReturnedAndVerifySysmanFabricPortGetCallSucceeds) {
    uint32_t count = 0U;
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, SysmanFabricPortFixture::numPorts);
}

TEST_F(SysmanFabricPortFixture, GivenPortCountCorrectWhenCallingZetSysmanFabricPortGetThenCountHandlesAreReturnedAndAndVerifySysmanFabricPortGetCallSucceeds) {
    uint32_t count = SysmanFabricPortFixture::numPorts;
    zet_sysman_fabric_port_handle_t hPorts[SysmanFabricPortFixture::numPorts];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, SysmanFabricPortFixture::numPorts);
}

TEST_F(SysmanFabricPortFixture, GivenPortCountGreaterThanPortsWhenCallingZetSysmanFabricPortGetThenCorrectCountisReturnedAndAndVerifySysmanFabricPortGetCallSucceeds) {
    uint32_t count = SysmanFabricPortFixture::numPorts + 1U;
    zet_sysman_fabric_port_handle_t hPorts[SysmanFabricPortFixture::numPorts + 1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, SysmanFabricPortFixture::numPorts);
}

TEST_F(SysmanFabricPortFixture, GivenPortCounLessThanPortsWhenCallingZetSysmanFabricPortGetThenCountLessTanPortsHandlesAreReturned) {
    uint32_t count = SysmanFabricPortFixture::numPorts - 1U;
    zet_sysman_fabric_port_handle_t hPorts[SysmanFabricPortFixture::numPorts - 1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, SysmanFabricPortFixture::numPorts - 1U);
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortGetPropertiesThenZetSysmanFabricPortGetPropertiesCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_port_properties_t properties;
    // Initialize values
    properties.onSubdevice = true;
    properties.subdeviceId = std::numeric_limits<uint32_t>::max();
    std::memset(properties.model, std::numeric_limits<int8_t>::max(), ZET_MAX_FABRIC_PORT_MODEL_SIZE);
    std::memset(properties.portUuid.id, std::numeric_limits<uint8_t>::max(), ZET_MAX_FABRIC_PORT_UUID_SIZE);
    properties.maxRxSpeed.bitRate = std::numeric_limits<uint64_t>::max();
    properties.maxRxSpeed.width = std::numeric_limits<uint32_t>::max();
    properties.maxRxSpeed.maxBandwidth = std::numeric_limits<uint64_t>::max();
    properties.maxTxSpeed.bitRate = std::numeric_limits<uint64_t>::max();
    properties.maxTxSpeed.width = std::numeric_limits<uint32_t>::max();
    properties.maxTxSpeed.maxBandwidth = std::numeric_limits<uint64_t>::max();

    result = zetSysmanFabricPortGetProperties(hPorts[0], &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(0L, properties.subdeviceId);
    EXPECT_STREQ("EXAMPLE", reinterpret_cast<char *>(properties.model));
    zet_fabric_port_uuid_t expectedUuid = {};
    EXPECT_TRUE(0 == std::memcmp(&expectedUuid, properties.portUuid.id, ZET_MAX_FABRIC_PORT_UUID_SIZE));
    EXPECT_EQ(0LU, properties.maxRxSpeed.bitRate);
    EXPECT_EQ(0U, properties.maxRxSpeed.width);
    EXPECT_EQ(0LU, properties.maxRxSpeed.maxBandwidth);
    EXPECT_EQ(0LU, properties.maxTxSpeed.bitRate);
    EXPECT_EQ(0U, properties.maxTxSpeed.width);
    EXPECT_EQ(0LU, properties.maxTxSpeed.maxBandwidth);
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortGetLinkTypeThenZetSysmanFabricPortGetLinkTypeCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_link_type_t linkType;
    bool verbose = true;
    result = zetSysmanFabricPortGetLinkType(hPorts[0U], verbose, &linkType);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    verbose = false;
    result = zetSysmanFabricPortGetLinkType(hPorts[0U], verbose, &linkType);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_STREQ("SAMPLE LINK", reinterpret_cast<char *>(linkType.desc));
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortGetConfigThenZetSysmanFabricPortGetConfigCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_port_config_t getConfig = {.enabled = true, .beaconing = true};
    result = zetSysmanFabricPortGetConfig(hPorts[0U], &getConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(getConfig.enabled);
    EXPECT_FALSE(getConfig.beaconing);
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortSetConfigThenZetSysmanFabricPortGetConfigCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_port_config_t setConfig = {.enabled = true, .beaconing = false};
    result = zetSysmanFabricPortSetConfig(hPorts[0U], &setConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zet_fabric_port_config_t getConfig = {.enabled = false, .beaconing = true};
    result = zetSysmanFabricPortGetConfig(hPorts[0U], &getConfig);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(getConfig.enabled);
    EXPECT_FALSE(getConfig.beaconing);
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortGetStateThenZetSysmanFabricPortGetStateCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_port_state_t state;
    result = zetSysmanFabricPortGetState(hPorts[0U], &state);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_FABRIC_PORT_STATUS_BLACK, state.status);
    EXPECT_EQ(ZET_FABRIC_PORT_QUAL_ISSUES_NONE, state.qualityIssues);
    EXPECT_EQ(ZET_FABRIC_PORT_STAB_ISSUES_NONE, state.stabilityIssues);
    EXPECT_EQ(0LU, state.rxSpeed.bitRate);
    EXPECT_EQ(0U, state.rxSpeed.width);
    EXPECT_EQ(0LU, state.rxSpeed.maxBandwidth);
    EXPECT_EQ(0LU, state.txSpeed.bitRate);
    EXPECT_EQ(0U, state.txSpeed.width);
    EXPECT_EQ(0LU, state.txSpeed.maxBandwidth);
}

TEST_F(SysmanFabricPortFixture, GivenValidFabricPortHandleWhenCallingZetSysmanFabricPortGetThroughputThenZetSysmanFabricPortGetThroughputCallSucceeds) {
    uint32_t count = 1U;
    zet_sysman_fabric_port_handle_t hPorts[1U];
    ze_result_t result = zetSysmanFabricPortGet(hSysman, &count, hPorts);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1U);

    zet_fabric_port_throughput_t throughput;
    // Initialize values
    throughput.timestamp = 0LU;
    throughput.rxCounter = std::numeric_limits<uint64_t>::max();
    throughput.txCounter = std::numeric_limits<uint64_t>::max();
    throughput.rxMaxBandwidth = std::numeric_limits<uint64_t>::max();
    throughput.txMaxBandwidth = std::numeric_limits<uint64_t>::max();

    result = zetSysmanFabricPortGetThroughput(hPorts[0U], &throughput);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0LU, throughput.timestamp);
    EXPECT_EQ(0LU, throughput.rxCounter);
    EXPECT_EQ(0LU, throughput.txCounter);
    EXPECT_EQ(0LU, throughput.rxMaxBandwidth);
    EXPECT_EQ(0LU, throughput.txMaxBandwidth);
}

} // namespace ult
} // namespace L0
