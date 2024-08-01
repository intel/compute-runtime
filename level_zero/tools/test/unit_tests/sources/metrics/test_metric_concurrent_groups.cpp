/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_source.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class ConcurrentMetricGroupFixture : public DeviceFixture,
                                     public ::testing::Test {
  public:
    std::unique_ptr<MetricDeviceContext> deviceContext = nullptr;

  protected:
    void SetUp() override;
    void TearDown() override;
};

void ConcurrentMetricGroupFixture::TearDown() {
    DeviceFixture::tearDown();
    deviceContext.reset();
}

void ConcurrentMetricGroupFixture::SetUp() {
    DeviceFixture::setUp();
    deviceContext = std::make_unique<MetricDeviceContext>(*device);
}

class MockMetricSourceType1 : public MockMetricSource {
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override {
        // No overlap possible
        if (*pConcurrentGroupCount == 0) {
            *pConcurrentGroupCount = static_cast<uint32_t>(hMetricGroups.size());
            return ZE_RESULT_SUCCESS;
        }

        *pConcurrentGroupCount = std::min(*pConcurrentGroupCount, static_cast<uint32_t>(hMetricGroups.size()));
        for (uint32_t index = 0; index < *pConcurrentGroupCount; index++) {
            pCountPerConcurrentGroup[index] = 1;
        }
        return ZE_RESULT_SUCCESS;
    }
};

class MockMetricSourceType2 : public MockMetricSource {
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override {
        // Requires the number of metric groups to be a multiple of 2
        if (hMetricGroups.size() & 1) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        // No overlap possible
        if (*pConcurrentGroupCount == 0) {
            *pConcurrentGroupCount = static_cast<uint32_t>(hMetricGroups.size());
            return ZE_RESULT_SUCCESS;
        }

        *pConcurrentGroupCount = std::min(*pConcurrentGroupCount, static_cast<uint32_t>(hMetricGroups.size() / 2));
        for (uint32_t index = 0; index < *pConcurrentGroupCount; index++) {
            pCountPerConcurrentGroup[index] = 2;
        }
        return ZE_RESULT_SUCCESS;
    }
};

TEST_F(ConcurrentMetricGroupFixture, WhenGetConcurrentMetricGroupsIsCalledForDifferentSourcesThenGeneratedGroupsAreCorrect) {

    MockMetricSourceType1 sourceType1;
    sourceType1.setType(1);
    MockMetricGroup metricGroupType1[] = {MockMetricGroup(sourceType1), MockMetricGroup(sourceType1), MockMetricGroup(sourceType1)};

    MockMetricSourceType2 sourceType2;
    sourceType2.setType(2);
    MockMetricGroup metricGroupType2[] = {MockMetricGroup(sourceType2), MockMetricGroup(sourceType2), MockMetricGroup(sourceType2), MockMetricGroup(sourceType2)};

    std::vector<zet_metric_group_handle_t> metricGroupHandles{};

    metricGroupHandles.push_back(metricGroupType1[0].toHandle());
    metricGroupHandles.push_back(metricGroupType2[1].toHandle());
    metricGroupHandles.push_back(metricGroupType1[1].toHandle());
    metricGroupHandles.push_back(metricGroupType2[3].toHandle());
    metricGroupHandles.push_back(metricGroupType1[2].toHandle());
    metricGroupHandles.push_back(metricGroupType2[0].toHandle());
    metricGroupHandles.push_back(metricGroupType2[2].toHandle());

    auto backupMetricGroupHandles = metricGroupHandles;

    uint32_t concurrentGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, nullptr));
    EXPECT_EQ(concurrentGroupCount, 4u);
    std::vector<uint32_t> countPerConcurrentGroup(concurrentGroupCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, countPerConcurrentGroup.data()));

    EXPECT_EQ(countPerConcurrentGroup[0], 3u);
    EXPECT_EQ(countPerConcurrentGroup[1], 3u);
    EXPECT_EQ(countPerConcurrentGroup[2], 1u);

    // Ensure that re-arranged metric group handles were originally present in the vector
    for (auto &metricGroupHandle : metricGroupHandles) {
        EXPECT_TRUE(std::find(backupMetricGroupHandles.begin(), backupMetricGroupHandles.end(), metricGroupHandle) != backupMetricGroupHandles.end());
    }
}

TEST_F(ConcurrentMetricGroupFixture, WhenGetConcurrentMetricGroupsIsCalledWithIncorrectGroupCountThenFailureIsReturned) {

    MockMetricSourceType1 sourceType1;
    sourceType1.setType(1);
    MockMetricGroup metricGroupType1[] = {MockMetricGroup(sourceType1), MockMetricGroup(sourceType1), MockMetricGroup(sourceType1)};

    MockMetricSourceType2 sourceType2;
    sourceType2.setType(2);
    MockMetricGroup metricGroupType2[] = {MockMetricGroup(sourceType2), MockMetricGroup(sourceType2), MockMetricGroup(sourceType2), MockMetricGroup(sourceType2)};

    std::vector<zet_metric_group_handle_t> metricGroupHandles{};

    metricGroupHandles.push_back(metricGroupType1[0].toHandle());
    metricGroupHandles.push_back(metricGroupType2[1].toHandle());
    metricGroupHandles.push_back(metricGroupType1[1].toHandle());
    metricGroupHandles.push_back(metricGroupType2[3].toHandle());
    metricGroupHandles.push_back(metricGroupType1[2].toHandle());
    metricGroupHandles.push_back(metricGroupType2[0].toHandle());
    metricGroupHandles.push_back(metricGroupType2[2].toHandle());

    uint32_t concurrentGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, nullptr));
    EXPECT_EQ(concurrentGroupCount, 4u);
    concurrentGroupCount -= 1;
    std::vector<uint32_t> countPerConcurrentGroup(concurrentGroupCount);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, countPerConcurrentGroup.data()));
    EXPECT_EQ(concurrentGroupCount, 0u);
}

class MockMetricSourceError : public MockMetricSource {
  public:
    static int32_t errorCallCount;
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override {
        errorCallCount -= 1;
        errorCallCount = std::max(0, errorCallCount);
        *pConcurrentGroupCount = 10;
        if (errorCallCount) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ~MockMetricSourceError() override {
        errorCallCount = 0;
    }
};

int32_t MockMetricSourceError::errorCallCount = 1;

TEST_F(ConcurrentMetricGroupFixture, WhenGetConcurrentMetricGroupsIsCalledAndSourceImplementationReturnsErrorThenErrorIsObserved) {

    MockMetricSourceError sourceTypeError;
    sourceTypeError.setType(1);
    MockMetricGroup metricGroupError[] = {MockMetricGroup(sourceTypeError),
                                          MockMetricGroup(sourceTypeError),
                                          MockMetricGroup(sourceTypeError)};

    std::vector<zet_metric_group_handle_t> metricGroupHandles{};

    metricGroupHandles.push_back(metricGroupError[0].toHandle());
    metricGroupHandles.push_back(metricGroupError[1].toHandle());
    metricGroupHandles.push_back(metricGroupError[1].toHandle());

    uint32_t concurrentGroupCount = 0;
    MockMetricSourceError::errorCallCount = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, nullptr));
    concurrentGroupCount = 10;
    std::vector<uint32_t> countPerConcurrentGroup(10);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, countPerConcurrentGroup.data()));
    EXPECT_EQ(concurrentGroupCount, 0u);

    MockMetricSourceError::errorCallCount = 1;
    concurrentGroupCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, deviceContext->getConcurrentMetricGroups(static_cast<uint32_t>(metricGroupHandles.size()), metricGroupHandles.data(), &concurrentGroupCount, nullptr));
    EXPECT_EQ(concurrentGroupCount, 0u);
}

} // namespace ult
} // namespace L0
