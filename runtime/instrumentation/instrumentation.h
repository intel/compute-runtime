/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

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

extern const bool haveInstrumentation;

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
    InstrReportDataMonitor Gp;
    InstrReportDataUser User;
    InstrReportDataOa Oa;
} InstrReportData;

typedef struct {
    uint32_t DMAFenceIdBegin;
    uint32_t DMAFenceIdEnd;
    uint32_t CoreFreqBegin;
    uint32_t CoreFreqEnd;
    InstrReportData HwPerfReportBegin;
    InstrReportData HwPerfReportEnd;
    uint32_t OaStatus;
    uint32_t OaHead;
    uint32_t OaTail;
} HwPerfCounters;

typedef struct {
    uint32_t Offset;
    uint32_t BitSize;
} InstrPmReg;

typedef struct {
    uint32_t Handle;
    uint32_t RegsCount;
} InstrPmRegsOaCountersCfg;

typedef struct {
    uint32_t Handle;
    uint32_t RegsCount;
} InstrPmRegsGpCountersCfg;

typedef struct {
    InstrPmReg Reg[INSTR_MAX_READ_REGS];
    uint32_t RegsCount;
} InstrReadRegsCfg;

typedef struct {
    InstrPmRegsOaCountersCfg OaCounters;
    InstrPmRegsGpCountersCfg GpCounters;
    InstrReadRegsCfg ReadRegs;
} InstrPmRegsCfg;

typedef struct {
    void *hAdapter;
    void *hDevice;
    void *pfnEscapeCb;
    bool DDI;
} InstrEscCbData;

bool instrAutoSamplingStart(
    InstrEscCbData cbData,
    void **ppOAInterface);

bool instrAutoSamplingStop(
    void **ppOAInterface);

bool instrCheckPmRegsCfg(
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

bool instrEscGetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t cfgId,
    InstrPmRegsCfg *pCfg,
    InstrAutoSamplingMode *pAutoSampling);

bool instrEscHwMetricsEnable(
    InstrEscCbData cbData,
    bool enable);

bool instrEscLoadPmRegsCfg(
    InstrEscCbData cbData,
    InstrPmRegsCfg *pCfg,
    bool hardwareAccess = 1);

bool instrEscSetPmRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pValues);

bool instrEscSendReadRegsCfg(
    InstrEscCbData cbData,
    uint32_t count,
    uint32_t *pOffsets,
    uint32_t *pBitSizes);

bool instrSetAvailable(bool enabled);

void instrEscVerifyEnable(
    InstrEscCbData cbData);

uint32_t instrSetPlatformInfo(
    uint32_t productId,
    void *pSkuTable);

} // namespace NEO
