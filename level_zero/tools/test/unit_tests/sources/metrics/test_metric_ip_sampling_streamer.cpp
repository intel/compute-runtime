/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {
extern _ze_driver_handle_t *globalDriverHandle;

namespace ult {

class MetricIpSamplingStreamerTest : public MetricIpSamplingFixture {

  public:
    zet_metric_group_handle_t getMetricGroup(L0::Device *device) {
        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);
        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data());
        EXPECT_NE(metricGroups[0], nullptr);
        return metricGroups[0];
    }
};

TEST_F(MetricIpSamplingStreamerTest, GivenAllInputsAreCorrectWhenStreamerOpenAndCloseAreCalledThenSuccessIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenEventHandleIsNullWhenStreamerOpenAndCloseAreCalledThenSuccessIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, nullptr, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenMetricGroupIsNotActivatedWhenStreamerOpenIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(
            zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle),
            ZE_RESULT_NOT_READY);
        EXPECT_EQ(streamerHandle, nullptr);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStreamerIsAlreadyOpenWhenStreamerOpenIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {
        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle0 = {}, streamerHandle1 = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle0), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle0, nullptr);
        EXPECT_EQ(
            zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle1),
            ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
        EXPECT_EQ(streamerHandle1, nullptr);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle0), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStartMeasurementFailsWhenStreamerOpenIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];
        std::size_t subDeviceIndex = index;
        if (!device->getNEODevice()->isSubDevice()) {
            ASSERT_GE(testDevices.size(), 3u);
            subDeviceIndex = 2;
        }

        osInterfaceVector[subDeviceIndex]->startMeasurementReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;

        EXPECT_EQ(
            zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle),
            osInterfaceVector[subDeviceIndex]->startMeasurementReturn);
        EXPECT_EQ(streamerHandle, nullptr);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStopMeasurementFailsWhenStreamerCloseIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];
        std::size_t subDeviceIndex = index;
        if (!device->getNEODevice()->isSubDevice()) {
            ASSERT_GE(testDevices.size(), 3u);
            subDeviceIndex = 2;
        }

        osInterfaceVector[subDeviceIndex]->stopMeasurementReturn = ZE_RESULT_ERROR_UNKNOWN;

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;

        EXPECT_EQ(
            zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle),
            ZE_RESULT_SUCCESS);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), osInterfaceVector[subDeviceIndex]->stopMeasurementReturn);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenAllInputsAreCorrectWhenReadDataIsCalledThenSuccessIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);

        size_t rawSize = 0;
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 100, &rawSize, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_NE(rawSize, 0u);
        std::vector<uint8_t> rawData(rawSize);
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 50, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenAllInputsAreCorrectWhenReadDataIsCalledWithMaxReportCountUint32MaxThenSuccessIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];
        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);

        size_t rawSize = 0;
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, UINT32_MAX, &rawSize, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_NE(rawSize, 0u);
        rawSize = 256;
        std::vector<uint8_t> rawData(rawSize);
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, UINT32_MAX, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenReadDataFromKmdFailsWhenStreamerReadDataIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];
        auto subDeviceIndex = index;

        if (!device->getNEODevice()->isSubDevice()) {
            ASSERT_GE(testDevices.size(), 2u);
            subDeviceIndex = 1;
        }
        osInterfaceVector[subDeviceIndex]->readDataReturn = ZE_RESULT_ERROR_UNKNOWN;

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);

        size_t rawSize = 0;
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 100, &rawSize, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_NE(rawSize, 0u);
        std::vector<uint8_t> rawData(rawSize);
        EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 50, &rawSize, rawData.data()),
                  osInterfaceVector[subDeviceIndex]->readDataReturn);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStreamerOpenIsSuccessfullWhenStreamerAppendMarkerIsCalledThenErrorIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);

        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        std::unique_ptr<L0::CommandList> commandList(
            CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
        EXPECT_EQ(zetCommandListAppendMetricStreamerMarker(commandList->toHandle(), streamerHandle, 0),
                  ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStreamerIsOpenAndDataIsAvailableToReadWhenEventQueryStatusIsCalledThenEventIsSignalled) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];

        if (!device->getNEODevice()->isSubDevice()) {
            ASSERT_GE(testDevices.size(), 2u);
            osInterfaceVector[1]->isNReportsAvailableReturn = true;
        } else {
            osInterfaceVector[index]->isNReportsAvailableReturn = true;
        }
        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;
        ze_event_pool_handle_t eventPoolHandle = {};
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = 0;
        eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

        ze_device_handle_t hDevice = device->toHandle();

        EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &hDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(eventPoolHandle, nullptr);

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
        EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(eventHandle, nullptr);

        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeEventQueryStatus(eventHandle));
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);

        EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenStreamerIsOpenAndDataIsNotAvailableToReadWhenEventQueryStatusIsCalledThenEventIsNotSignalled) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (std::size_t index = 0; index < testDevices.size(); index++) {

        auto device = testDevices[index];
        osInterfaceVector[index]->isNReportsAvailableReturn = false;

        if (!device->getNEODevice()->isSubDevice()) {
            ASSERT_GE(testDevices.size(), 3u);
            osInterfaceVector[1]->isNReportsAvailableReturn = false;
            osInterfaceVector[2]->isNReportsAvailableReturn = false;
        } else {
            osInterfaceVector[index]->isNReportsAvailableReturn = false;
        }
        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;

        ze_event_pool_handle_t eventPoolHandle = {};
        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = 0;
        eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

        ze_device_handle_t hDevice = device->toHandle();

        EXPECT_EQ(zeEventPoolCreate(context->toHandle(), &eventPoolDesc, 1, &hDevice, &eventPoolHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(eventPoolHandle, nullptr);

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
        EXPECT_EQ(zeEventCreate(eventPoolHandle, &eventDesc, &eventHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(eventHandle, nullptr);

        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);
        EXPECT_NE(ZE_RESULT_SUCCESS, zeEventQueryStatus(eventHandle));
        EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);

        EXPECT_EQ(zeEventDestroy(eventHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(zeEventPoolDestroy(eventPoolHandle), ZE_RESULT_SUCCESS);
    }
}

TEST_F(MetricIpSamplingStreamerTest, GivenAllInputsAreCorrectWhenReadDataIsCalledOnRootDeviceThenCorrectDataIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto device = testDevices[0];

    ASSERT_FALSE(device->getNEODevice()->isSubDevice());
    ASSERT_GE(testDevices.size(), 3u);

    zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    ze_event_handle_t eventHandle = {};
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};
    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Setup data for both subdevices
    osInterfaceVector[1]->isfillDataEnabled = true;
    osInterfaceVector[1]->fillData = 2;
    osInterfaceVector[1]->fillDataSize = 64 * 20;

    osInterfaceVector[2]->isfillDataEnabled = true;
    osInterfaceVector[2]->fillData = 4;
    osInterfaceVector[2]->fillDataSize = 64 * 30;

    size_t rawSize = 0;
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 100, &rawSize, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_NE(rawSize, 0u);
    std::vector<uint8_t> rawData = {};
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 75, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(rawSize, osInterfaceVector[1]->fillDataSize + osInterfaceVector[2]->fillDataSize + sizeof(IpSamplingMetricDataHeader) * 2u);
    auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(rawData.data());
    EXPECT_EQ(header->magic, IpSamplingMetricDataHeader::magicValue);
    EXPECT_EQ(header->rawDataSize, osInterfaceVector[1]->fillDataSize);
    EXPECT_EQ(header->setIndex, 0u);

    const auto subDeviceDataOffset = sizeof(IpSamplingMetricDataHeader) + osInterfaceVector[1]->fillDataSize;
    header = reinterpret_cast<IpSamplingMetricDataHeader *>(rawData.data() + subDeviceDataOffset);
    EXPECT_EQ(header->magic, IpSamplingMetricDataHeader::magicValue);
    EXPECT_EQ(header->rawDataSize, osInterfaceVector[2]->fillDataSize);
    EXPECT_EQ(header->setIndex, 1u);

    const auto rawDataStartOffset = sizeof(IpSamplingMetricDataHeader);
    EXPECT_EQ(rawData[rawDataStartOffset], 2u);
    EXPECT_EQ(rawData[rawDataStartOffset + osInterfaceVector[1]->fillDataSize - 1], 2u);
    EXPECT_EQ(rawData[subDeviceDataOffset + rawDataStartOffset], 4u);
    EXPECT_EQ(rawData[subDeviceDataOffset + rawDataStartOffset + osInterfaceVector[2]->fillDataSize - 1], 4u);
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricIpSamplingStreamerTest, GivenSubDeviceMetricHandleWhenCallingZetContextActivateMetricGroupsWithRootDeviceMetricGroupsThenCallFails) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto device = testDevices[0];

    ASSERT_FALSE(device->getNEODevice()->isSubDevice());
    ASSERT_GE(testDevices.size(), 3u);

    auto subDevice = testDevices[1];
    ASSERT_TRUE(subDevice->getNEODevice()->isSubDevice());

    zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), subDevice, 1, &metricGroupHandle), ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_F(MetricIpSamplingStreamerTest, GivenNotEnoughMemoryWhileReadingWhenReadDataIsCalledOnRootDeviceThenCorrectDataIsReturned) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    auto device = testDevices[0];

    ASSERT_FALSE(device->getNEODevice()->isSubDevice());
    ASSERT_GE(testDevices.size(), 3u);

    zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
    EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

    ze_event_handle_t eventHandle = {};
    zet_metric_streamer_handle_t streamerHandle = {};
    zet_metric_streamer_desc_t streamerDesc = {};
    streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
    streamerDesc.notifyEveryNReports = 32768;
    streamerDesc.samplingPeriod = 1000;
    EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
    EXPECT_NE(streamerHandle, nullptr);

    // Setup data for both subdevices
    osInterfaceVector[1]->isfillDataEnabled = true;
    osInterfaceVector[1]->fillData = 2;
    osInterfaceVector[1]->fillDataSize = osInterfaceVector[1]->getUnitReportSize() * 20;

    osInterfaceVector[2]->isfillDataEnabled = true;
    osInterfaceVector[2]->fillData = 4;
    osInterfaceVector[2]->fillDataSize = osInterfaceVector[2]->getUnitReportSize() * 30;

    size_t rawSize = 0;
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 100, &rawSize, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_NE(rawSize, 0u);

    std::vector<uint8_t> rawData = {};
    // Setup memory for only the first sub-device's read to succeed
    rawSize = osInterfaceVector[1]->fillDataSize;
    rawData.resize(rawSize);
    EXPECT_EQ(zetMetricStreamerReadData(streamerHandle, 75, &rawSize, rawData.data()), ZE_RESULT_SUCCESS);
    // Expect less than the fillDataSize, since Header size needs to be accounted for
    EXPECT_LT(rawSize, osInterfaceVector[1]->fillDataSize);
    const auto rawDataStartOffset = sizeof(IpSamplingMetricDataHeader);
    EXPECT_EQ(rawData[rawDataStartOffset + 0], 2u);
    const auto maxReportCount = (osInterfaceVector[1]->fillDataSize -
                                 sizeof(IpSamplingMetricDataHeader) * 2) /
                                osInterfaceVector[1]->getUnitReportSize();
    const auto firstSubDevicelastUpdatedByteOffset = sizeof(IpSamplingMetricDataHeader) + maxReportCount * osInterfaceVector[1]->getUnitReportSize();
    EXPECT_EQ(rawData[firstSubDevicelastUpdatedByteOffset - 1], 2u);
    EXPECT_NE(rawData[firstSubDevicelastUpdatedByteOffset], 2u);
    EXPECT_EQ(zetMetricStreamerClose(streamerHandle), ZE_RESULT_SUCCESS);
}

TEST_F(MetricIpSamplingStreamerTest, whenGetConcurrentMetricGroupsIsCalledThenCorrectConcurrentGroupsAreRetrieved) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());
    for (auto device : testDevices) {

        auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        std::vector<zet_metric_group_handle_t> metricGroupList{};
        metricGroupList.push_back(metricGroupHandle);

        uint32_t concurrentGroupCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, metricSource.getConcurrentMetricGroups(metricGroupList, &concurrentGroupCount, nullptr));
        EXPECT_EQ(concurrentGroupCount, 1u);

        std::vector<uint32_t> countPerConcurrentGroup(concurrentGroupCount);
        concurrentGroupCount += 1;
        EXPECT_EQ(ZE_RESULT_SUCCESS, metricSource.getConcurrentMetricGroups(metricGroupList, &concurrentGroupCount, countPerConcurrentGroup.data()));
        EXPECT_EQ(concurrentGroupCount, 1u);
    }
}

} // namespace ult
} // namespace L0
