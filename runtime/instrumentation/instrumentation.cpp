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

#include "instrumentation.h"

namespace OCLRT {
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
    void *pSkuTable) {
    return 0;
}

} // namespace OCLRT
