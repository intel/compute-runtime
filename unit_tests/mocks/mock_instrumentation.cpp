/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "instrumentation.h"

#include <cstdint>

using namespace NEO;

bool InstrAutoSamplingStart(
    InstrEscCbData cbData,
    void **ppOAInterface) {

    return false;
}

bool InstrAutoSamplingStop(
    void **ppOAInterface) {

    return false;
}

bool InstrCheckPmRegsCfg(
    InstrPmRegsCfg *pQueryPmRegsCfg,
    uint32_t *pLastPmRegsCfgHandle,
    const void *pASInterface) {

    return false;
}

bool InstrEscGetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t cfgId,
    InstrPmRegsCfg *pCfg,
    InstrAutoSamplingMode *pAutoSampling) {
    return false;
}
bool InstrEscHwMetricsEnable(
    InstrEscCbData cbData,
    bool enable) {
    return false;
}

bool InstrEscLoadPmRegsCfg(
    InstrEscCbData cbData,
    InstrPmRegsCfg *pCfg,
    bool hardwareAccess) {
    return false;
}

bool InstrEscSetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pValues) {
    return false;
}

bool InstrEscSendReadRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pBitSizes) {
    return false;
}

uint32_t InstrSetPlatformInfo(
    uint32_t productId,
    void *featureTable) {
    return -1;
}

bool InstrSetAvailable(bool enabled) {
    return false;
}

void InstrEscVerifyEnable(InstrEscCbData cbData) {
}
