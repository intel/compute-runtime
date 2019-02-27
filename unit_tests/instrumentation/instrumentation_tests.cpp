/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "instrumentation.h"

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
    InstrPmRegsCfg cfg;
    instrCheckPmRegsCfg(&cfg, pLastPmRegsCfgHandle, pASInterface);
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
    bool enable = false;
    instrEscHwMetricsEnable(cbData, enable);
}

TEST(InstrumentationTest, instrEscLoadPmRegsCfg) {
    InstrEscCbData cbData = {0};
    InstrPmRegsCfg *pCfg = nullptr;
    bool hardwareAccess = false;
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
    bool enabled = false;
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
