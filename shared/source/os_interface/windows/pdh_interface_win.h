/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/pdh_interface.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <Pdh.h>
#include <mutex>
#include <string>
#include <unordered_map>

namespace NEO {
class ExecutionEnvironment;
class Wddm;

using PdhOpenQueryWFn = PDH_STATUS(__stdcall *)(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH_HQUERY *phQuery);
using PdhAddEnglishCounterWFn = PDH_STATUS(__stdcall *)(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER *phCounter);
using PdhCollectQueryDataFn = PDH_STATUS(__stdcall *)(PDH_HQUERY hQuery);
using PdhGetFormattedCounterValueFn = PDH_STATUS(__stdcall *)(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue);
using PdhCloseQueryFn = PDH_STATUS(__stdcall *)(PDH_HQUERY hQuery);

class PdhInterfaceWindows : public PdhInterface {
  public:
    explicit PdhInterfaceWindows(ExecutionEnvironment &executionEnvironment);
    ~PdhInterfaceWindows() override;
    uint64_t getCurrentMemoryUsage(uint32_t rootDeviceIndex, bool dedicatedCounter) override;

  protected:
    bool initPdh();
    std::wstring constructPdhCounterString(const std::wstring &objectName, const std::wstring &counterName, LUID luid, uint32_t index) const;
    Wddm &getWddm(uint32_t rootDeviceIndex) const;

    ExecutionEnvironment &executionEnvironment;
    PdhOpenQueryWFn pdhOpenQueryFunction = nullptr;
    PdhAddEnglishCounterWFn pdhAddEnglishCounterWFunction = nullptr;
    PdhCollectQueryDataFn pdhCollectQueryDataFunction = nullptr;
    PdhGetFormattedCounterValueFn pdhGetFormattedCounterValueFunction = nullptr;
    PdhCloseQueryFn pdhCloseQueryFunction = nullptr;
    std::unique_ptr<OsLibrary> pdhLibrary;
    std::once_flag pdhInitOnceFlag;
    bool pdhInitialized = false;
    PDH_HQUERY memoryUsageQuery = nullptr;
    std::unordered_map<std::wstring, PDH_HCOUNTER> counterHandles;
};

} // namespace NEO
