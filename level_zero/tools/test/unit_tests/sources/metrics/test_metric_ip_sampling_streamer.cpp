/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/metric_ip_sampling_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_ip_sampling.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"
#include <level_zero/zet_api.h>

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
        DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
        size_t expectedSize = reportSize * UINT32_MAX;
        expectedSize *= (!deviceImp->isSubdevice && deviceImp->isImplicitScalingCapable()) ? deviceImp->numSubDevices : 1u;
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

using MetricIpSamplingMetricSupportedScopeTest = MetricIpSamplingStreamerTest;

class MockMetricImp : public MetricImp {
  public:
    using MetricImp::MetricImp;

    void setScopes(const std::vector<zet_intel_metric_scope_exp_handle_t> &newScopes) {
        scopes = newScopes;
    }
};

TEST_F(MetricIpSamplingMetricSupportedScopeTest, givenMetricWhenGettingSupportedMetricScopesThenExpectedCountAndHandlesAreReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    EXPECT_EQ(testDevices[0]->getMetricDeviceContext().getMetricScopes().size(), 0u);

    zet_metric_group_handle_t metricGroupHandle = MetricIpSamplingStreamerTest::getMetricGroup(testDevices[0]);
    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    metricCount = 1;
    zet_metric_handle_t phMetric{};
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, &phMetric), ZE_RESULT_SUCCESS);

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesHandles;
    MockMetricScope *mockMetricScope = new MockMetricScope(scopeProperties, false);
    metricScopesHandles.push_back(mockMetricScope->toHandle());

    auto metricImp = static_cast<MockMetricImp *>(Metric::fromHandle(phMetric));
    metricImp->setScopes(metricScopesHandles);

    uint32_t metricScopesCount = 0;
    EXPECT_EQ(zetIntelMetricSupportedScopesGetExp(&phMetric, &metricScopesCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricScopesCount, 1u);

    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesHandle(metricScopesCount);
    EXPECT_EQ(zetIntelMetricSupportedScopesGetExp(&phMetric, &metricScopesCount, metricScopesHandle.data()), ZE_RESULT_SUCCESS);
    EXPECT_NE(metricScopesHandle[0], nullptr);

    delete mockMetricScope;
}

using MetricIpSamplingCalcOpMultiDevTest = MetricIpSamplingCalculateMultiDevFixture;

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingMetricGroupThenCreateAndDestroyCalcOpIsSuccessful, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        ze_device_properties_t props = {};
        device->getProperties(&props);

        uint32_t metricGroupCount = 1;
        zet_metric_group_handle_t metricGroupHandle = nullptr;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);

        calculationDesc.metricGroupCount = 1;
        calculationDesc.phMetricGroups = &metricGroupHandle;
        calculationDesc.metricCount = 0;
        calculationDesc.phMetrics = nullptr;

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calculationDesc,
                                                                                 &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, 10u);
        zet_metric_handle_t phMetric{};
        metricCount = 1;
        EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, &phMetric), ZE_RESULT_SUCCESS);

        calculationDesc.metricGroupCount = 0;
        calculationDesc.metricCount = 1;
        calculationDesc.phMetrics = &phMetric;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calculationDesc,
                                                                                 &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingMetricGroupCreatingCalcOpIgnoresTimeFilters, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        ze_device_properties_t props = {};
        device->getProperties(&props);

        uint32_t metricGroupCount = 1;
        zet_metric_group_handle_t metricGroupHandle = nullptr;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);

        calculationDesc.metricGroupCount = 1;
        calculationDesc.phMetricGroups = &metricGroupHandle;
        calculationDesc.timeAggregationWindow = 1000;

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calculationDesc,
                                                              &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        calculationDesc.timeWindowsCount = 1;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calculationDesc,
                                                              &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

        calculationDesc.timeAggregationWindow = 0;
        EXPECT_EQ(ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP,
                  zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                              device->toHandle(), &calculationDesc,
                                                              &hCalculationOperation));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingCalcOpCanGetReportFormat, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 1;
        zet_metric_group_handle_t metricGroupHandle = nullptr;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);

        calculationDesc.metricGroupCount = 1;
        calculationDesc.phMetricGroups = &metricGroupHandle;

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calculationDesc,
                                                                                 &hCalculationOperation));

        uint32_t metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
        EXPECT_EQ(metricsInReportCount, 10u);

        std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
        std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                          &metricsInReportCount,
                                                                                          metricsInReport.data(),
                                                                                          metricScopesInReport.data()));
        EXPECT_EQ(metricsInReportCount, 10u);

        zet_metric_properties_t ipSamplingMetricProperties = {};

        for (uint32_t i = 0; i < metricsInReportCount; i++) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
            EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[i].c_str()), 0);
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

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 1;
        zet_metric_group_handle_t metricGroupHandle = nullptr;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);

        uint32_t metricCount = 0;
        EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricCount, 10u);
        std::vector<zet_metric_handle_t> phMetrics(metricCount);
        EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, phMetrics.data()), ZE_RESULT_SUCCESS);

        std::vector<zet_metric_handle_t> metricsToCalculate;

        // select only odd metrics
        for (uint32_t i = 0; i < metricCount; i++) {
            if (i % 2) {
                metricsToCalculate.push_back(phMetrics[i]);
            }
        }

        uint32_t metricsToCalculateCount = static_cast<uint32_t>(metricsToCalculate.size());
        EXPECT_EQ(metricsToCalculateCount, 5u);

        calculationDesc.metricCount = metricsToCalculateCount;
        calculationDesc.phMetrics = metricsToCalculate.data();

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calculationDesc,
                                                                                 &hCalculationOperation));

        uint32_t metricsInReportCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
        EXPECT_EQ(metricsInReportCount, 5u);

        std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
        std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                          &metricsInReportCount,
                                                                                          metricsInReport.data(),
                                                                                          metricScopesInReport.data()));
        EXPECT_EQ(metricsInReportCount, 5u);

        // Metrics must be in the report but can be in different order
        for (auto metric : metricsToCalculate) {
            auto it = std::find(metricsInReport.begin(), metricsInReport.end(), metric);
            EXPECT_NE(it, metricsInReport.end());
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingMultiScopeCalcOpGetReportFormatIncludesSelectedMetrics, EustallSupportedPlatforms) {

    zet_intel_metric_scope_properties_exp_t scopeProperties{};
    scopeProperties.stype = ZET_STRUCTURE_TYPE_INTEL_METRIC_SCOPE_PROPERTIES_EXP;
    scopeProperties.pNext = nullptr;

    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesHandles;

    // Make compute scope be first in the scopes input array to calculationDesc
    scopeProperties.iD = 1;
    MockMetricScope *mockMetricScope1 = new MockMetricScope(scopeProperties, false);
    metricScopesHandles.push_back(mockMetricScope1->toHandle());

    // Make aggregated scope be second
    scopeProperties.iD = 0;
    MockMetricScope *mockMetricScope2 = new MockMetricScope(scopeProperties, true);
    metricScopesHandles.push_back(mockMetricScope2->toHandle());

    auto device = testDevices[0];
    ze_device_properties_t props = {};
    device->getProperties(&props);
    EXPECT_EQ(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());

    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t metricGroupHandle = nullptr;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &metricGroupHandle;
    calculationDesc.metricScopesCount = 2;
    calculationDesc.phMetricScopes = metricScopesHandles.data();

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));

    uint32_t metricsInReportCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
    EXPECT_EQ(metricsInReportCount, 20u);

    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 20u);

    zet_metric_properties_t ipSamplingMetricProperties = {};

    for (uint32_t i = 0; i < metricsInReportCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &ipSamplingMetricProperties));
        uint32_t expectedMetricIndex = i % static_cast<uint32_t>(expectedMetricNamesInReport.size());
        EXPECT_EQ(strcmp(ipSamplingMetricProperties.name, expectedMetricNamesInReport[expectedMetricIndex].c_str()), 0);
        MetricScopeImp *scope = static_cast<MetricScopeImp *>(MetricScope::fromHandle(metricScopesInReport[i]));

        // Aggregated scope should always come first in the report format
        if (i < 10) {
            EXPECT_TRUE(scope->isAggregated());
        } else {
            EXPECT_FALSE(scope->isAggregated());
        }
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));

    metricScopesHandles.clear();
    delete mockMetricScope1;
    delete mockMetricScope2;
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, givenIpSamplingCalcOpCanGetExcludedMetricsAreZero, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 1;
        zet_metric_group_handle_t metricGroupHandle = nullptr;
        EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
        EXPECT_EQ(metricGroupCount, 1u);
        EXPECT_NE(metricGroupHandle, nullptr);

        calculationDesc.metricGroupCount = 1;
        calculationDesc.phMetricGroups = &metricGroupHandle;

        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                                 device->toHandle(), &calculationDesc,
                                                                                 &hCalculationOperation));

        uint32_t excludedMetricsCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, &excludedMetricsCount, nullptr));
        EXPECT_EQ(excludedMetricsCount, 0u);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, WhenReadingMetricGroupTimeCalculateFilterThenCorrectValueIsReturned, EustallSupportedPlatforms) {

    EXPECT_EQ(ZE_RESULT_SUCCESS, testDevices[0]->getMetricDeviceContext().enableMetricApi());

    for (auto device : testDevices) {

        uint32_t metricGroupCount = 0;
        zetMetricGroupGet(device->toHandle(), &metricGroupCount, nullptr);
        EXPECT_EQ(metricGroupCount, 1u);

        std::vector<zet_metric_group_handle_t> metricGroups;
        metricGroups.resize(metricGroupCount);

        ASSERT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, metricGroups.data()), ZE_RESULT_SUCCESS);
        ASSERT_NE(metricGroups[0], nullptr);

        zet_intel_metric_group_calculation_properties_exp_t metricGroupCalcProps{};
        metricGroupCalcProps.stype = ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATION_EXP_PROPERTIES;
        metricGroupCalcProps.pNext = nullptr;
        metricGroupCalcProps.isTimeFilterSupported = true;

        zet_metric_group_properties_t properties{};
        properties.pNext = &metricGroupCalcProps;

        EXPECT_EQ(zetMetricGroupGetProperties(metricGroups[0], &properties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(strcmp(properties.description, "EU stall sampling"), 0);
        EXPECT_EQ(strcmp(properties.name, "EuStallSampling"), 0);
        EXPECT_EQ(metricGroupCalcProps.isTimeFilterSupported, false);
    }
}

HWTEST2_F(MetricIpSamplingCalcOpMultiDevTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnRootDeviceExpectSuccess, EustallSupportedPlatforms) {

    auto device = testDevices[0];
    ze_device_properties_t props = {};
    device->getProperties(&props);
    EXPECT_EQ(props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getMetricDeviceContext().enableMetricApi());

    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t metricGroupHandle = nullptr;
    EXPECT_EQ(zetMetricGroupGet(device->toHandle(), &metricGroupCount, &metricGroupHandle), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricGroupCount, 1u);
    EXPECT_NE(metricGroupHandle, nullptr);

    calculationDesc.metricGroupCount = 1;
    calculationDesc.phMetricGroups = &metricGroupHandle;

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));

    std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
    addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricCalculateValuesExp(rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                                    hCalculationOperation, final, &usedSize,
                                                                                    &totalMetricReportCount, nullptr));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                                 hCalculationOperation, final, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

using MetricIpSamplingCalcOpTest = MetricIpSamplingCalculateSingleDevFixture;

HWTEST2_F(MetricIpSamplingCalcOpTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceThenSuccessIsReturned, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 10;
    std::vector<zet_metric_handle_t> metricsInReport(metricsInReportCount);
    std::vector<zet_intel_metric_scope_exp_handle_t> metricScopesInReport(metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation,
                                                                                      &metricsInReportCount,
                                                                                      metricsInReport.data(),
                                                                                      metricScopesInReport.data()));
    EXPECT_EQ(metricsInReportCount, 10u);
    std::vector<zet_metric_properties_t> metricProperties(metricsInReportCount);

    for (uint32_t i = 0; i < metricsInReportCount; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zetMetricGetProperties(metricsInReport[i], &metricProperties[i]));
    }

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);
    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * metricsInReportCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                  hCalculationOperation, final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, rawDataVectorSize);

    for (uint32_t i = 0; i < totalMetricReportCount; i++) {
        for (uint32_t j = 0; j < metricsInReportCount; j++) {
            uint32_t resultIndex = i * metricsInReportCount + j;
            EXPECT_TRUE(metricProperties[j].resultType == expectedMetricValues[resultIndex].type);
            EXPECT_TRUE(metricResults[resultIndex].value.ui64 == expectedMetricValues[resultIndex].value.ui64);
            EXPECT_TRUE(metricResults[resultIndex].resultStatus == ZET_INTEL_METRIC_CALCULATION_EXP_RESULT_VALID);
        }
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceThenHandleErrorConditions, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));

    std::vector<uint8_t> rawDataWithHeader(rawDataVectorSize + sizeof(IpSamplingMetricDataHeader));
    addHeader(rawDataWithHeader.data(), rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataVector.data()), rawDataVectorSize, 0);

    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    // sub-device cal op does not accept root device data
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateValuesExp(rawDataWithHeader.size(), reinterpret_cast<uint8_t *>(rawDataWithHeader.data()),
                                                                                 hCalculationOperation,
                                                                                 final, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));

    // invalid input raw data size
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zetIntelMetricCalculateValuesExp(rawDataVectorSize / 30, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                             hCalculationOperation,
                                                                             final, &usedSize,
                                                                             &totalMetricReportCount, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceCanDetectRawDataOverflow, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t totalMetricReportCount = 2;
    bool final = true;
    size_t usedSize = 0;
    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * 10);

    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zetIntelMetricCalculateValuesExp(rawDataVectorOverflowSize, reinterpret_cast<uint8_t *>(rawDataVectorOverflow.data()),
                                                                               hCalculationOperation,
                                                                               final, &usedSize,
                                                                               &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 2U);
    EXPECT_EQ(usedSize, rawDataVectorOverflowSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesOnSubDeviceFilterMetricsInReport, EustallSupportedPlatforms) {

    uint32_t metricCount = 0;
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(metricCount, 10u);
    std::vector<zet_metric_handle_t> phMetrics(metricCount);
    EXPECT_EQ(zetMetricGet(metricGroupHandle, &metricCount, phMetrics.data()), ZE_RESULT_SUCCESS);

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

    calculationDesc.metricGroupCount = 0;
    calculationDesc.phMetricGroups = nullptr;
    calculationDesc.metricCount = metricsToCalculateCount;
    calculationDesc.phMetrics = metricsToCalculate.data();

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t metricsInReportCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, &metricsInReportCount, nullptr, nullptr));
    EXPECT_EQ(metricsInReportCount, 5u);

    uint32_t totalMetricReportCount = 0;

    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));

    std::vector<zet_intel_metric_result_exp_t> metricResults(totalMetricReportCount * metricsInReportCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, metricResults.data()));
    EXPECT_EQ(totalMetricReportCount, 3U);
    EXPECT_EQ(usedSize, rawDataVectorSize);

    // Expect only odd index metrics results of the first report
    for (uint32_t j = 0; j < metricsInReportCount; j++) {
        EXPECT_TRUE(metricResults[j].value.ui64 == expectedMetricValues[j * 2 + 1].value.ui64);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

HWTEST2_F(MetricIpSamplingCalcOpTest, GivenIpSamplingCalcOpCallingMetricCalculateValuesDoesNotAllowLimitingResults, EustallSupportedPlatforms) {

    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationCreateExp(context->toHandle(),
                                                                             device->toHandle(), &calculationDesc,
                                                                             &hCalculationOperation));
    uint32_t totalMetricReportCount = 0;
    bool final = true;
    size_t usedSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                  hCalculationOperation,
                                                                  final, &usedSize,
                                                                  &totalMetricReportCount, nullptr));
    EXPECT_EQ(totalMetricReportCount, 3U);

    // Request fewer reports
    totalMetricReportCount = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zetIntelMetricCalculateValuesExp(rawDataVectorSize, reinterpret_cast<uint8_t *>(rawDataVector.data()),
                                                                                 hCalculationOperation,
                                                                                 final, &usedSize,
                                                                                 &totalMetricReportCount, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation));
}

} // namespace ult
} // namespace L0
