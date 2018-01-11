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

#pragma once

#include <cstdint>

namespace OCLRT {

constexpr unsigned int INSTR_GENERAL_PURPOSE_COUNTERS_COUNT = 4;
constexpr unsigned int INSTR_MAX_USER_COUNTERS_COUNT = 32;
constexpr unsigned int INSTR_MMIO_NOOPID = 0x2094;
constexpr unsigned int INSTR_MMIO_RPSTAT1 = 0xA01C;

constexpr unsigned int INSTR_GTDI_MAX_READ_REGS = 16;
constexpr unsigned int INSTR_GTDI_PERF_METRICS_OA_COUNT = 36;
constexpr unsigned int INSTR_GTDI_PERF_METRICS_OA_40b_COUNT = 32;
constexpr unsigned int INSTR_GTDI_PERF_METRICS_NOA_COUNT = 16;
constexpr unsigned int INSTR_MAX_CONTEXT_TAGS = 128;

constexpr unsigned int INSTR_MAX_OA_PROLOG = 2;
constexpr unsigned int INSTR_MAX_OA_EPILOG = 2;
constexpr unsigned int INSTR_MAX_PM_REGS_BASE = 256;
constexpr unsigned int INSTR_MAX_PM_REGS = (INSTR_MAX_PM_REGS_BASE + INSTR_MAX_OA_PROLOG + INSTR_MAX_OA_EPILOG);

constexpr unsigned int INSTR_PM_REGS_CFG_INVALID = 0;
constexpr unsigned int INSTR_READ_REGS_CFG_TAG = 0xFFFFFFFE;
constexpr unsigned int INSTR_MAX_READ_REGS = 16;

typedef enum {
    INSTR_AS_MODE_OFF,
    INSTR_AS_MODE_EVENT,
    INSTR_AS_MODE_TIMER,
    INSTR_AS_MODE_DMA
} InstrAutoSamplingMode;

typedef enum GTDI_CONFIGURATION_SET {
    GTDI_CONFIGURATION_SET_DYNAMIC = 0,
    GTDI_CONFIGURATION_SET_1,
    GTDI_CONFIGURATION_SET_2,
    GTDI_CONFIGURATION_SET_3,
    GTDI_CONFIGURATION_SET_4,
    GTDI_CONFIGURATION_SET_COUNT,
    GTDI_CONFIGURATION_SET_MAX = 0xFFFFFFFF
} GTDI_CONFIGURATION_SET;

enum INSTR_GFX_OFFSETS {
    INSTR_PERF_CNT_1_DW0 = 0x91B8,
    INSTR_PERF_CNT_1_DW1 = 0x91BC,
    INSTR_PERF_CNT_2_DW0 = 0x91C0,
    INSTR_PERF_CNT_2_DW1 = 0x91C4,
    INSTR_OA_STATUS = 0x2B08,
    INSTR_OA_HEAD_PTR = 0x2B0C,
    INSTR_OA_TAIL_PTR = 0x2B10
};

typedef struct {

} GTDI_QUERY;

typedef struct {
    uint32_t contextId[INSTR_MAX_CONTEXT_TAGS];
    uint32_t count;
} InstrAllowedContexts;

typedef struct {
    uint64_t counter[INSTR_GTDI_MAX_READ_REGS];
    uint32_t userCntrCfgId;
} InstrReportDataUser;

typedef struct {
    uint32_t reportId;
    uint32_t timestamp;
    uint32_t contextId;
    uint32_t gpuTicksCounter;
} InstrReportDataOaHeader;

typedef struct {
    uint32_t oaCounter[INSTR_GTDI_PERF_METRICS_OA_COUNT];
    uint8_t oaCounterHB[INSTR_GTDI_PERF_METRICS_OA_40b_COUNT];
    uint32_t noaCounter[INSTR_GTDI_PERF_METRICS_NOA_COUNT];
} InstrReportDataOaData;

typedef struct {
    InstrReportDataOaHeader header;
    InstrReportDataOaData data;
} InstrReportDataOa;

typedef struct {
    uint64_t counter1;
    uint64_t counter2;
} InstrReportDataMonitor;

typedef struct {
    InstrReportDataMonitor gp;
    InstrReportDataUser user;
    InstrReportDataOa oa;
} InstrReportData;

typedef struct {
    uint32_t dmaFenceIdBegin;
    uint32_t dmaFenceIdEnd;
    uint32_t coreFreqBegin;
    uint32_t coreFreqEnd;
    InstrReportData reportBegin;
    InstrReportData reportEnd;
    uint32_t oaStatus;
    uint32_t oaHead;
    uint32_t oaTail;
} HwPerfCounters;

typedef struct {
    uint32_t offset;
    union {
        uint32_t value32;
        uint64_t value64;
    };
    uint32_t bitSize;
    uint32_t flags;
} InstrPmReg;

typedef struct {
    uint32_t handle;
    InstrPmReg reg[INSTR_MAX_PM_REGS];
    uint32_t regsCount;
    uint32_t pendingRegsCount;
} InstrPmRegsOaCountersCfg;

typedef struct {
    uint32_t handle;
    InstrPmReg reg[INSTR_MAX_PM_REGS];
    uint32_t regsCount;
    uint32_t pendingRegsCount;
} InstrPmRegsGpCountersCfg;

typedef struct {
    uint32_t handle;
    InstrPmReg reg[INSTR_MAX_READ_REGS];
    uint32_t regsCount;
    uint32_t srmsCount;
} InstrReadRegsCfg;

typedef struct {
    InstrPmRegsOaCountersCfg oaCounters;
    InstrPmRegsGpCountersCfg gpCounters;
    InstrReadRegsCfg readRegs;
} InstrPmRegsCfg;

typedef struct {
    void *hAdapter;
    void *hDevice;
    void *pfnEscapeCb;
    bool DDI;
} InstrEscCbData;

int instrAutoSamplingStart(
    InstrEscCbData cbData,
    void **ppOAInterface);

int instrAutoSamplingStop(
    void **ppOAInterface);

int instrCheckPmRegsCfg(
    InstrPmRegsCfg *pQueryPmRegsCfg,
    uint32_t *pLastPmRegsCfgHandle,
    const void *pASInterface);

void instrGetPerfCountersQueryData(
    InstrEscCbData cbData,
    GTDI_QUERY *pData,
    HwPerfCounters *pLayout,
    uint64_t cpuRawTimestamp,
    void *pASInterface,
    InstrPmRegsCfg *pPmRegsCfg,
    bool useMiRPC,
    bool resetASData = false,
    const InstrAllowedContexts *pAllowedContexts = nullptr);

int instrEscGetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t cfgId,
    InstrPmRegsCfg *pCfg,
    InstrAutoSamplingMode *pAutoSampling);

int instrEscHwMetricsEnable(
    InstrEscCbData cbData,
    int enable);

int instrEscLoadPmRegsCfg(
    InstrEscCbData cbData,
    InstrPmRegsCfg *pCfg,
    int hardwareAccess = 1);

int instrEscSetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pValues);

int instrEscSendReadRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pBitSizes);

int instrSetAvailable(int enabled);

void instrEscVerifyEnable(
    InstrEscCbData cbData);

uint64_t instrSetPlatformInfo(
    uint32_t productId,
    void *pSkuTable);

} // namespace OCLRT
