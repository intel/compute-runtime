/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmQueryTest, GivenGtMaxFreqFileExistsWhenFrequencyIsQueriedThenValidValueIsReturned) {
    int expectedMaxFrequency = 1000;

    DrmMock drm{};
    auto hwInfo = *defaultHwInfo;

    std::string gtMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt_max_freq_mhz";

    EXPECT_TRUE(fileExists(gtMaxFreqFile));
    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}
