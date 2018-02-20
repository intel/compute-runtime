/*
 * Copyright (c) 2018, Intel Corporation
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

#include <cstdint>
#include "instrumentation.h"

using namespace OCLRT;

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
    void *pSkuTable) {
    return -1;
}

bool InstrSetAvailable(bool enabled) {
    return false;
}

void InstrEscVerifyEnable(InstrEscCbData cbData) {
}
