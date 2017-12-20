/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/cpu_info.h"
#include "gtest/gtest.h"

using namespace OCLRT;

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