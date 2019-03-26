/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/hw_info.h"

#include "CL/cl.h"

#include <memory>
#include <mutex>

namespace NEO {
struct HardwareInfo;
class OSInterface;
class OSTime;

class PerformanceCounters {
  public:
    static std::unique_ptr<PerformanceCounters> create(OSTime *osTime);
    virtual ~PerformanceCounters() = default;
    void enable();
    void shutdown();
    virtual void initialize(const HardwareInfo *hwInfo);
    InstrPmRegsCfg *getPmRegsCfg(uint32_t configuration);
    bool sendPmRegsCfgCommands(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending);
    void setCpuTimestamp();
    bool processEventReport(size_t pClientDataSize, void *pClientData, size_t *outputSize, HwPerfCounter *pPrivateData, InstrPmRegsCfg *countersConfiguration, bool isEventComplete);
    int sendPerfConfiguration(uint32_t count, uint32_t *pOffsets, uint32_t *pValues);
    uint32_t getCurrentReportId();

    uint32_t getPerfCountersReferenceNumber() {
        mutex.lock();
        std::lock_guard<std::mutex> lg(mutex, std::adopt_lock);

        return refCounter;
    }

    bool isAvailable() {
        return available;
    }

  protected:
    PerformanceCounters(OSTime *osTime);
    virtual bool verifyPmRegsCfg(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending);
    virtual void enableImpl();
    void shutdownImpl();
    MOCKABLE_VIRTUAL uint32_t getReportId() {
        return ++reportId & 0xFFF;
    }
    GFXCORE_FAMILY gfxFamily;
    InstrEscCbData cbData;
    OSInterface *osInterface;
    OSTime *osTime;
    bool hwMetricsEnabled;
    bool useMIRPC;
    void *pAutoSamplingInterface;
    uint64_t cpuRawTimestamp;
    std::mutex mutex;
    uint32_t refCounter;
    bool available;
    uint32_t reportId;
    decltype(&instrAutoSamplingStart) autoSamplingStartFunc = instrAutoSamplingStart;
    decltype(&instrAutoSamplingStop) autoSamplingStopFunc = instrAutoSamplingStop;
    decltype(&instrCheckPmRegsCfg) checkPmRegsCfgFunc = instrCheckPmRegsCfg;
    decltype(&instrGetPerfCountersQueryData) getPerfCountersQueryDataFunc = instrGetPerfCountersQueryData;
    decltype(&instrEscGetPmRegsCfg) getPmRegsCfgFunc = instrEscGetPmRegsCfg;
    decltype(&instrEscHwMetricsEnable) hwMetricsEnableFunc = instrEscHwMetricsEnable;
    decltype(&instrEscLoadPmRegsCfg) loadPmRegsCfgFunc = instrEscLoadPmRegsCfg;
    decltype(&instrEscSetPmRegsCfg) setPmRegsCfgFunc = instrEscSetPmRegsCfg;
    decltype(&instrEscSendReadRegsCfg) sendReadRegsCfgFunc = instrEscSendReadRegsCfg;
};
} // namespace NEO
