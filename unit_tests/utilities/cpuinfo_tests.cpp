/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/cpu_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CpuInfo, detectsSSE4) {
    const CpuInfo &cpuInfo = CpuInfo::getInstance();
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureFpu));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureCmov));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureMmx));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureFxsave));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSse));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSsE2));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSsE3));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSssE3));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSsE41));
    EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::featureSsE42));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_MOVBE));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_POPCNT));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_PCLMULQDQ));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_AES));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_F16C));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_AVX));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_RDRND));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_FMA));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_BMI));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_LZCNT));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_HLE));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_RTM));
    //EXPECT_TRUE(cpuInfo.isFeatureSupported(CpuInfo::_FEATURE_AVX2));
}

TEST(CpuInfo, cpuidex) {
    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    uint32_t cpuRegsInfo[4];
    uint32_t subleaf = 0;
    cpuInfo.cpuidex(cpuRegsInfo, 4, subleaf);
}