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

#include "runtime/instrumentation/instrumentation.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace OCLRT;

struct InstrumentationTest : public ::testing::Test {
    InstrumentationTest() {}
};

TEST(InstrumentationTest, instrAutoSamplingStart) {
    InstrEscCbData cbData = {0};
    void **ppOAInterface = nullptr;
    instrAutoSamplingStart(cbData, ppOAInterface);
}

TEST(InstrumentationTest, instrAutoSamplingStop) {
    void **ppOAInterface = nullptr;
    instrAutoSamplingStop(ppOAInterface);
}

TEST(InstrumentationTest, instrCheckPmRegsCfg) {
    InstrPmRegsCfg *pQueryPmRegsCfg = nullptr;
    uint32_t *pLastPmRegsCfgHandle = nullptr;
    const void *pASInterface = nullptr;
    instrCheckPmRegsCfg(pQueryPmRegsCfg, pLastPmRegsCfgHandle, pASInterface);
}

TEST(InstrumentationTest, instrGetPerfCountersQueryData) {
    InstrEscCbData cbData = {0};
    GTDI_QUERY *pData = nullptr;
    HwPerfCounters *pLayout = nullptr;
    uint64_t cpuRawTimestamp = 0;
    void *pASInterface = nullptr;
    InstrPmRegsCfg *pPmRegsCfg = nullptr;
    bool useMiRPC = false;
    bool resetASData = false;
    const InstrAllowedContexts *pAllowedContexts = nullptr;
    instrGetPerfCountersQueryData(cbData, pData, pLayout, cpuRawTimestamp, pASInterface, pPmRegsCfg, useMiRPC, resetASData, pAllowedContexts);
}

TEST(InstrumentationTest, instrEscGetPmRegsCfg) {
    InstrEscCbData cbData = {0};
    uint32_t cfgId = 0;
    InstrPmRegsCfg *pCfg = nullptr;
    InstrAutoSamplingMode *pAutoSampling = nullptr;
    instrEscGetPmRegsCfg(cbData, cfgId, pCfg, pAutoSampling);
}

TEST(InstrumentationTest, instrEscHwMetricsEnable) {
    InstrEscCbData cbData = {0};
    int enable = 0;
    instrEscHwMetricsEnable(cbData, enable);
}

TEST(InstrumentationTest, instrEscLoadPmRegsCfg) {
    InstrEscCbData cbData = {0};
    InstrPmRegsCfg *pCfg = nullptr;
    int hardwareAccess = 0;
    instrEscLoadPmRegsCfg(cbData, pCfg, hardwareAccess);
}

TEST(InstrumentationTest, instrEscSetPmRegsCfg) {
    InstrEscCbData cbData = {0};
    uint32_t count = 0;
    uint32_t *pOffsets = nullptr;
    uint32_t *pValues = nullptr;
    instrEscSetPmRegsCfg(cbData, count, pOffsets, pValues);
}

TEST(InstrumentationTest, instrEscSendReadRegsCfg) {
    InstrEscCbData cbData = {0};
    uint32_t count = 0;
    uint32_t *pOffsets = nullptr;
    uint32_t *pBitSizes = nullptr;
    instrEscSendReadRegsCfg(cbData, count, pOffsets, pBitSizes);
}

TEST(InstrumentationTest, instrSetAvailable) {
    int enabled = 0;
    instrSetAvailable(enabled);
}

TEST(InstrumentationTest, instrEscVerifyEnable) {
    InstrEscCbData cbData = {0};
    instrEscVerifyEnable(cbData);
}

TEST(InstrumentationTest, instrSetPlatformInfo) {
    uint32_t productId = 0;
    void *pSkuTable = nullptr;
    instrSetPlatformInfo(productId, pSkuTable);
}
