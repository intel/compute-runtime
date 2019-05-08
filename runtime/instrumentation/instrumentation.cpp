/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "instrumentation.h"

namespace NEO {
const bool haveInstrumentation = false;

bool instrAutoSamplingStart(
    InstrEscCbData cbData,
    void **ppOAInterface) {
    return false;
}

bool instrAutoSamplingStop(
    void **ppOAInterface) {
    return false;
}

bool instrCheckPmRegsCfg(
    InstrPmRegsCfg *pQueryPmRegsCfg,
    uint32_t *pLastPmRegsCfgHandle,
    const void *pASInterface) {
    return false;
}

void instrGetPerfCountersQueryData(
    InstrEscCbData cbData,
    GTDI_QUERY *pData,
    HwPerfCounters *pLayout,
    uint64_t cpuRawTimestamp,
    void *pASInterface,
    InstrPmRegsCfg *pPmRegsCfg,
    bool useMiRPC,
    bool resetASData,
    const InstrAllowedContexts *pAllowedContexts) {
}

bool instrEscGetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t cfgId,
    InstrPmRegsCfg *pCfg,
    InstrAutoSamplingMode *pAutoSampling) {
    return false;
}

bool instrEscHwMetricsEnable(
    InstrEscCbData cbData,
    bool enable) {
    return false;
}

bool instrEscLoadPmRegsCfg(
    InstrEscCbData cbData,
    InstrPmRegsCfg *pCfg,
    bool hardwareAccess) {
    return false;
}

bool instrEscSetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pValues) {
    return false;
}

bool instrEscSendReadRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pBitSizes) {
    return false;
}

bool instrSetAvailable(bool enabled) {
    return false;
}

void instrEscVerifyEnable(
    InstrEscCbData cbData) {
}

uint32_t instrSetPlatformInfo(
    uint32_t productId,
    void *featureTable) {
    return 0;
}

} // namespace NEO
