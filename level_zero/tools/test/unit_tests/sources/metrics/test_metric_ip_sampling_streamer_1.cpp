/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"

namespace L0 {
extern _ze_driver_handle_t *globalDriverHandle;

namespace ult {

class MetricIpSamplingStreamerTest : public MetricIpSamplingMultiDevFixture {

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

TEST_F(MetricIpSamplingStreamerTest, GivenAllInputsAreCorrectWhenStreamerOpenWithHwBufferSizeQueryIsCalledExpectedSizeIsReturned) {

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

        zet_intel_metric_hw_buffer_size_exp_desc_t hwBufferSizeDesc{};
        hwBufferSizeDesc.sizeInBytes = 128u;
        hwBufferSizeDesc.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_HW_BUFFER_SIZE_EXP_DESC;
        hwBufferSizeDesc.pNext = nullptr;

        streamerDesc.pNext = &hwBufferSizeDesc;
        EXPECT_EQ(zetMetricStreamerOpen(context->toHandle(), device, metricGroupHandle, &streamerDesc, eventHandle, &streamerHandle), ZE_RESULT_SUCCESS);
        EXPECT_NE(streamerHandle, nullptr);

        const uint32_t reportSize = 64;
        Device *l0Device = static_cast<Device *>(device);
        size_t expectedSize = reportSize * UINT32_MAX;
        expectedSize *= (!l0Device->isSubdevice && l0Device->isImplicitScalingCapable()) ? l0Device->numSubDevices : 1u;
        EXPECT_EQ(hwBufferSizeDesc.sizeInBytes, expectedSize);
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

        zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(device);
        EXPECT_EQ(zetContextActivateMetricGroups(context->toHandle(), device, 1, &metricGroupHandle), ZE_RESULT_SUCCESS);

        ze_event_handle_t eventHandle = {};
        zet_metric_streamer_handle_t streamerHandle = {};
        zet_metric_streamer_desc_t streamerDesc = {};
        streamerDesc.stype = ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC;
        streamerDesc.notifyEveryNReports = 32768;
        streamerDesc.samplingPeriod = 1000;

        osInterfaceVector[subDeviceIndex]->startMeasurementReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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
    EXPECT_EQ(rawSize, osInterfaceVector[1]->fillDataSize + osInterfaceVector[2]->fillDataSize + sizeof(IpSamplingMultiDevDataHeader) * 2u);
    auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawData.data());
    EXPECT_EQ(header->magic, IpSamplingMultiDevDataHeader::magicValue);
    EXPECT_EQ(header->rawDataSize, osInterfaceVector[1]->fillDataSize);
    EXPECT_EQ(header->setIndex, 0u);

    const auto subDeviceDataOffset = sizeof(IpSamplingMultiDevDataHeader) + osInterfaceVector[1]->fillDataSize;
    header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawData.data() + subDeviceDataOffset);
    EXPECT_EQ(header->magic, IpSamplingMultiDevDataHeader::magicValue);
    EXPECT_EQ(header->rawDataSize, osInterfaceVector[2]->fillDataSize);
    EXPECT_EQ(header->setIndex, 1u);

    const auto rawDataStartOffset = sizeof(IpSamplingMultiDevDataHeader);
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
    const auto rawDataStartOffset = sizeof(IpSamplingMultiDevDataHeader);
    EXPECT_EQ(rawData[rawDataStartOffset + 0], 2u);
    const auto maxReportCount = (osInterfaceVector[1]->fillDataSize -
                                 sizeof(IpSamplingMultiDevDataHeader) * 2) /
                                osInterfaceVector[1]->getUnitReportSize();
    const auto firstSubDevicelastUpdatedByteOffset = sizeof(IpSamplingMultiDevDataHeader) + maxReportCount * osInterfaceVector[1]->getUnitReportSize();
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

using MetricIpSamplingCalcOpMultiDevTest = MetricIpSamplingCalculateMultiDevFixture;

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenMetricWhenGettingSupportedMetricScopesThenExpectedCountAndHandlesAreReturned, EustallSupportedPlatforms) {

    for (auto device : testDevices) {

        uint32_t expectedScopesCount = 1u;
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            expectedScopesCount = 2u; // Root device supports 2 scopes,
            auto &l0GfxCoreHelper = device->getMetricDeviceContext().getDevice().getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
            if (l0GfxCoreHelper.supportMetricsAggregation()) {
                expectedScopesCount = 3u; // Root device supports 3 scopes when aggregation is supported
            }
        }

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, 10u);
        std::vector<zet_metric_handle_t> hMetrics(metricCount);
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, hMetrics.data()), ZE_RESULT_SUCCESS);
        for (auto &metric : hMetrics) {
            uint32_t metricScopesCount = 0;
            EXPECT_EQ(zetIntelMetricSupportedScopesGetExp(metric, &metricScopesCount, nullptr), ZE_RESULT_SUCCESS);
            EXPECT_EQ(metricScopesCount, expectedScopesCount);
            std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesHandle(metricScopesCount);
            EXPECT_EQ(zetIntelMetricSupportedScopesGetExp(metric, &metricScopesCount, metricScopesHandle.data()), ZE_RESULT_SUCCESS);
            EXPECT_EQ(metricScopesCount, expectedScopesCount);

            for (auto &scopeHandle : metricScopesHandle) {
                zet_intel_metric_scope_properties_exp_t scopeProperties{};
                scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
                scopeProperties.pNext = nullptr;
                EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricScopeGetPropertiesExp(scopeHandle,
                                                                                 &scopeProperties));

                if ((props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
                    EXPECT_EQ(scopeProperties.iD, 0u);
                } else {
                    EXPECT_TRUE((scopeProperties.iD == 0u) || (scopeProperties.iD == 1u) || (scopeProperties.iD == 2u));
                }
            }
        }
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingMetricGroupThenCreateAndDestroyCalcOpIsSuccessful, EustallSupportedPlatforms) {

    for (auto device : testDevices) {
        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calcDescPerDevice[device],
                                                                                 &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, 10u);
        zet_metric_handle_t phMetric{};
        metricCount = 1;
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, &phMetric), ZE_RESULT_SUCCESS);
        calcDescPerDevice[device].metricGroupCount = 0;
        calcDescPerDevice[device].phMetricGroups = nullptr;
        calcDescPerDevice[device].metricCount = 1;
        calcDescPerDevice[device].phMetrics = &phMetric;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calcDescPerDevice[device],
                                                                                 &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenMetricGroupGetFailsWhenCreatingCalcOpThenErrorIsReturned, EustallSupportedPlatforms) {

    for (auto device : testDevices) {

        ze_device_properties_t props = {};
        device->getProperties(&props);

        auto mockMetricSource = std::make_unique<MockMetricIpSamplingSource>(device->getMetricDeviceContext());

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation = nullptr;

        calcDescPerDevice[device].metricGroupCount = 1;
        calcDescPerDevice[device].phMetricGroups = nullptr;
        calcDescPerDevice[device].metricCount = 0;
        calcDescPerDevice[device].phMetrics = nullptr;

        bool isMultiDevice = !(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
        ze_result_t result = IpSamplingMetricCalcOpImp::create(isMultiDevice, *mockMetricSource, &calcDescPerDevice[device], &hCalculationOperation);

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingMetricGroupCreatingCalcOpIgnoresTimeFilters, EustallSupportedPlatforms) {

    for (auto device : testDevices) {
        calcDescPerDevice[device].timeAggregationWindow = 1000;
        calcDescPerDevice[device].timeWindowsCount = 1;

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calcDescPerDevice[device],
                                                              &hCalculationOperation));
        EXPECT_EQ(calcDescPerDevice[device].timeWindowsCount, 0u);
        EXPECT_EQ(calcDescPerDevice[device].timeAggregationWindow, 0u);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        calcDescPerDevice[device].timeWindowsCount = 1;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calcDescPerDevice[device],
                                                              &hCalculationOperation));
        EXPECT_EQ(calcDescPerDevice[device].timeWindowsCount, 0u);
        EXPECT_EQ(calcDescPerDevice[device].timeAggregationWindow, 0u);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        calcDescPerDevice[device].timeAggregationWindow = 100;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calcDescPerDevice[device],
                                                              &hCalculationOperation));
        EXPECT_EQ(calcDescPerDevice[device].timeWindowsCount, 0u);
        EXPECT_EQ(calcDescPerDevice[device].timeAggregationWindow, 0u);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingCalcOpCanGetReportFormat, EustallSupportedPlatforms) {

    for (auto device : testDevices) {

        uint32_t expectedMetricsInReportCount = 10u;
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            expectedMetricsInReportCount = 20u; // Root device supports 2 scopes, one per sub-device
        }

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calcDescPerDevice[device],
                                                                                 &hCalculationOperation));
        EXPECT_EQ(calcDescPerDevice[device].timeWindowsCount, 0u);
        EXPECT_EQ(calcDescPerDevice[device].timeAggregationWindow, 0u);
        uint32_t metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
        EXPECT_EQ(metricsInReportCount, expectedMetricsInReportCount);

        std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
        std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                          &metricsInReportCount,
                                                                                          metricsInReport.data(),
                                                                                          metricScopesInReport.data()));
        EXPECT_EQ(metricsInReportCount, expectedMetricsInReportCount);

        zet_metric_properties_t ipSamplingMetricProperties = {};

        for (uint32_t i = 0; i < metricsInReportCount; i++) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[i % expectedMetricNamesInReport.size()].c_str()), 0);
        }

        // Can't filter metrics in report
        metricsInReportCount = 1;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                                         &metricsInReportCount,
                                                                                                         metricsInReport.data(),
                                                                                                         metricScopesInReport.data()));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingCalcOpGetReportFormatIncludesSelectedMetrics, EustallSupportedPlatforms) {

    for (auto device : testDevices) {

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, 10u);
        std::vector<zet_metric_handle_t> phMetrics(metricCount);
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, phMetrics.data()), ZE_RESULT_SUCCESS);

        std::vector<zet_metric_handle_t> metricsToCalculate;

        // select only odd metrics
        for (uint32_t i = 0; i < metricCount; i++) {
            if (i % 2) {
                metricsToCalculate.push_back(phMetrics[i]);
            }
        }

        uint32_t expectedMetricsInReportCount = 5u;
        ze_device_properties_t props = {};
        device->getProperties(&props);
        if (!(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE)) {
            expectedMetricsInReportCount = 10u; // Root device supports 2 scopes, one per sub-device
        }

        uint32_t metricsToCalculateCount = static_cast<uint32_t>(metricsToCalculate.size());
        EXPECT_EQ(metricsToCalculateCount, 5u);

        calcDescPerDevice[device].metricGroupCount = 0;
        calcDescPerDevice[device].phMetricGroups = nullptr;
        calcDescPerDevice[device].metricCount = metricsToCalculateCount;
        calcDescPerDevice[device].phMetrics = metricsToCalculate.data();

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calcDescPerDevice[device],
                                                                                 &hCalculationOperation));
        EXPECT_EQ(calcDescPerDevice[device].timeWindowsCount, 0u);
        EXPECT_EQ(calcDescPerDevice[device].timeAggregationWindow, 0u);
        uint32_t metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
        EXPECT_EQ(metricsInReportCount, expectedMetricsInReportCount);

        std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
        std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                          &metricsInReportCount,
                                                                                          metricsInReport.data(),
                                                                                          metricScopesInReport.data()));
        EXPECT_EQ(metricsInReportCount, expectedMetricsInReportCount);

        // Metrics must be in the report but can be in different order
        for (auto metric : metricsToCalculate) {
            auto it = std::find(metricsInReport.begin(), metricsInReport.end(), metric);
            EXPECT_NE(it, metricsInReport.end());
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, GivenIpSamplingRootDeviceCalcOpCalculationDoesNotAcceptSubDeviceData, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calcDescPerDevice[rootDevice],
                                                                             &hCalculationOperation));
    EXPECT_EQ(calcDescPerDevice[rootDevice].timeWindowsCount, 0u);
    EXPECT_EQ(calcDescPerDevice[rootDevice].timeAggregationWindow, 0u);
    uint32_t totalMetricReportCount = 0;
    bool lastCall = true;
    size_t usedSize = 0;
    // root device cal op does not accept sub device data
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateValuesExp(rawReports.size(), reinterpret_cast<uint8_t *>(rawReports.data()),
                                                                                 hCalculationOperation,
                                                                                 lastCall, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingCalcOpCanGetExcludedMetricsAreZero, EustallSupportedPlatforms) {

    for (auto device : testDevices) {

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calcDescPerDevice[device],
                                                                                 &hCalculationOperation));

        uint32_t excludedMetricsCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, &excludedMetricsCount, nullptr));
        EXPECT_EQ(excludedMetricsCount, 0u);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, WhenReadingMetricGroupTimeCalculateFilterThenCorrectValueIsReturned, EustallSupportedPlatforms) {

    for (auto device : testDevices) {
        zet_intel_metric_group_calculation_properties_exp_t metricGroupCalcProps{};
        metricGroupCalcProps.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATION_EXP_PROPERTIES;
        metricGroupCalcProps.pNext = nullptr;
        metricGroupCalcProps.isTimeFilterSupported = true;

        zet_metric_group_properties_t properties{};
        properties.pNext = &metricGroupCalcProps;

        EXPECT_EQ(zetMetricGroupGetProperties(metricGroupHandlePerDevice[device], &properties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(properties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(properties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricGroupCalcProps.isTimeFilterSupported, false);
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, GivenMetricNotSupportingRequestedScopeWhenValidatingThenMetricIsExcluded, EustallSupportedPlatforms) {

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;
    scopeProperties.iD = 0;
    MockMetricScope *mockMetricScope = new MockMetricScope(scopeProperties, false, 0);
    std::vector<zet_intel_metric_scope_exp_handle_t> mockScopeHandles = {mockMetricScope->toHandle()};

    for (auto device : testDevices) {
        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[device], &metricCount, nullptr), ZE_RESULT_SUCCESS);

        // Request a Mock scope (no metrics support it - all should be excluded)
        calcDescPerDevice[device].metricScopesCount = static_cast<uint32_t>(mockScopeHandles.size());
        calcDescPerDevice[device].phMetricScopes = mockScopeHandles.data();

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation = nullptr;

        auto result = zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                  device->toHandle(), &calcDescPerDevice[device],
                                                                  &hCalculationOperation);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_NE(hCalculationOperation, nullptr);

        uint32_t excludedMetricCount = 0;
        EXPECT_EQ(zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, &excludedMetricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(excludedMetricCount, metricCount);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
    delete mockMetricScope;
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, GivenMultipleMetricsWithDifferentScopeSupportWhenValidatingThenOnlyUnsupportedAreExcluded, EustallSupportedPlatforms) {

    // ***  Test is only for root device since sub-device calculate operation only supports one scope ***

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;
    scopeProperties.iD = 1;
    MockMetricScope *mockMetricScope1 = new MockMetricScope(scopeProperties, false, 0);

    scopeProperties.iD = 2;
    MockMetricScope *mockMetricScope2 = new MockMetricScope(scopeProperties, false, 0);

    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[rootDevice], &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 10u);
    std::vector<zet_metric_handle_t> hMetrics(metricCount);
    EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[rootDevice], &metricCount, hMetrics.data()), ZE_RESULT_SUCCESS);

    // Metric 0 supports both scopes
    std::vector<zet_intel_metric_scope_exp_handle_t> supportedScopes1 = {mockMetricScope1->toHandle(), mockMetricScope2->toHandle()};
    auto metric0 = static_cast<MockMetric *>(Metric::fromHandle(hMetrics[0]));
    metric0->setScopes(supportedScopes1);

    // Metric 1 supports only scope1
    std::vector<zet_intel_metric_scope_exp_handle_t> supportedScopes2 = {mockMetricScope1->toHandle()};
    auto metric1 = static_cast<MockMetric *>(Metric::fromHandle(hMetrics[1]));
    metric1->setScopes(supportedScopes2);

    // Metric 2 supports only scope2
    std::vector<zet_intel_metric_scope_exp_handle_t> supportedScopes3 = {mockMetricScope2->toHandle()};
    auto metric2 = static_cast<MockMetric *>(Metric::fromHandle(hMetrics[2]));
    metric2->setScopes(supportedScopes3);

    // Set remaining metrics to support both scopes
    for (uint32_t i = 3; i < metricCount; i++) {
        auto metric = static_cast<MockMetric *>(Metric::fromHandle(hMetrics[i]));
        metric->setScopes(supportedScopes1);
    }

    // Request both scopes - metrics 1 and 2 should be excluded
    std::vector<zet_intel_metric_scope_exp_handle_t> scopeHandles = {
        mockMetricScope1->toHandle(),
        mockMetricScope2->toHandle()};

    calcDescPerDevice[rootDevice].metricGroupCount = 1;
    calcDescPerDevice[rootDevice].phMetricGroups = &metricGroupHandlePerDevice[rootDevice];
    calcDescPerDevice[rootDevice].metricScopesCount = static_cast<uint32_t>(scopeHandles.size());
    calcDescPerDevice[rootDevice].phMetricScopes = scopeHandles.data();

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation = nullptr;
    auto result = zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              rootDevice->toHandle(), &calcDescPerDevice[rootDevice],
                                                              &hCalculationOperation);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(hCalculationOperation, nullptr);

    uint32_t excludedMetricCount = 0;
    EXPECT_EQ(zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, &excludedMetricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(excludedMetricCount, 2u); // Metrics 1 and 2 should be excluded

    std::vector<zet_metric_handle_t> hExcludedMetrics(excludedMetricCount);
    EXPECT_EQ(zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, &excludedMetricCount, hExcludedMetrics.data()), ZE_RESULT_SUCCESS);
    for (auto &excludedMetric : hExcludedMetrics) {
        EXPECT_TRUE((excludedMetric == hMetrics[1]) || (excludedMetric == hMetrics[2]));
    }

    uint32_t reportMetricCount = 0;
    EXPECT_EQ(zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &reportMetricCount, nullptr, nullptr), ZE_RESULT_SUCCESS);
    // Report format includes all metrics from group * number of scopes (excluded metrics are filtered out)
    EXPECT_EQ(reportMetricCount, (metricCount - 2) * 2);

    std::vector<zet_metric_handle_t> hMetricsInreport(reportMetricCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(reportMetricCount);
    EXPECT_EQ(zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &reportMetricCount, hMetricsInreport.data(), metricScopesInReport.data()), ZE_RESULT_SUCCESS);
    for (auto &metricInReport : hMetricsInreport) {
        EXPECT_NE(metricInReport, hMetrics[1]);
        EXPECT_NE(metricInReport, hMetrics[2]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

    delete mockMetricScope1;
    delete mockMetricScope2;
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, GivenRootDeviceCreatingCalcOpWithOnlyMetricsHandlesOnlyThoseMetricsAreInResultReport, EustallSupportedPlatforms) {

    uint32_t metricGroupCount = 1;
    ASSERT_EQ(zetMetricGroupGet(rootDevice->toHandle(), &metricGroupCount, &metricGroupHandlePerDevice[rootDevice]), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    ASSERT_NE(metricGroupHandlePerDevice[rootDevice], nullptr);

    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[rootDevice], &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 10u);
    std::vector<zet_metric_handle_t> phMetrics(metricCount);
    EXPECT_EQ(zetMetricGet(metricGroupHandlePerDevice[rootDevice], &metricCount, phMetrics.data()), ZE_RESULT_SUCCESS);

    std::vector<zet_metric_handle_t> metricsToCalculate;

    // Select only odd index metrics. According to expectedMetricNamesInReport there are:
    // "Active", "PipeStall" "DistStall",  "SyncStall", "OtherStall"
    for (uint32_t i = 0; i < metricCount; i++) {
        if (i % 2) {
            metricsToCalculate.push_back(phMetrics[i]);
        }
    }

    uint32_t metricsToCalculateCount = static_cast<uint32_t>(metricsToCalculate.size());
    EXPECT_EQ(metricsToCalculateCount, 5u);

    calcDescPerDevice[rootDevice].metricGroupCount = 0;
    calcDescPerDevice[rootDevice].phMetricGroups = nullptr;
    calcDescPerDevice[rootDevice].metricCount = metricsToCalculateCount;
    calcDescPerDevice[rootDevice].phMetrics = metricsToCalculate.data();

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             rootDevice->toHandle(), &calcDescPerDevice[rootDevice],
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
    EXPECT_EQ(metricsInReportCount, 10u); // Root device supports 2 scopes, one per sub-device. So 5 metrics * 2 scopes = 10
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount,
                                                                                      metricsInReport.data(), metricScopesInReport.data()));

    EXPECT_EQ(metricsInReportCount, 10u);
    // Expect only odd index metrics in the result report
    zet_metric_properties_t ipSamplingMetricProperties = {};
    for (uint32_t i = 0; i < metricsInReportCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
        // i goes from 0 to 9, want odd indexes: 1, 3, 5, 7, 9, then repeat for second scope
        uint32_t metricIndex = (i % 5) * 2 + 1;
        EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[metricIndex].c_str()), 0);
        EXPECT_EQ(static_cast<MockMetricScope *>(MetricScope::fromHandle(metricScopesInReport[i])), scopesPerDevice[rootDevice][i / 5]); // each scope repeated 5 times
    }

    // Raw data for a single read with different data for sub-device 0 and 1
    size_t rawDataSize = sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader) + rawReportsBytesSize;
    std::vector<uint8_t> rawDataWithHeader(rawDataSize);
    // sub device index 0
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 0);
    // sub device index 1
    MockRawDataHelper::addMultiSubDevHeader(rawDataWithHeader.data() + rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader),
                                            rawDataWithHeader.size() - (rawReportsBytesSize + sizeof(IpSamplingMultiDevDataHeader)),
                                            reinterpret_cast<uint8_t *>(rawReports.data()), rawReportsBytesSize, 1);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));

    EXPECT_EQ(totalMetricReportCount, 3U); // three IPs in rawReports
    EXPECT_EQ(usedSize, 0U);               // query only, no data processed

    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataSize, reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, rawDataSize);

    for (uint32_t i = 0; i < totalMetricReportCount; i++) {
        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            uint32_t resultIndex = i * metricsInReportCount + j;
            EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValuesOddMetricsTwoScopes[resultIndex].value.ui64);
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

} // namespace ult
} // namespace L0
