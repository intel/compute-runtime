/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/zet_intel_gpu_metric.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(MetricTracerTest, WhenTracerRelatedApisAreCalledThenReturnUnsupportedFeature) {

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerCreateExp(nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerDestroyExp(nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerEnableExp(nullptr, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerDisableExp(nullptr, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerReadDataExp(nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricDecoderCreateExp(nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricDecoderDestroyExp(nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricDecoderGetDecodableMetricsExp(nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetMetricTracerDecodeExp(nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr));
    size_t rawDataSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricDecodeCalculateMultipleValuesExp(nullptr,
                                                                                                  rawDataSize, nullptr, nullptr, nullptr,
                                                                                                  nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zetIntelMetricTracerDecodeExp2(nullptr, rawDataSize, nullptr, 0, nullptr, 0, nullptr, 0, 0, nullptr));
}

} // namespace ult
} // namespace L0