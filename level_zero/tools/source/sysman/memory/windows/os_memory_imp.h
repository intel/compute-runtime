/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/memory/os_memory.h"
#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define UNICODE
#endif

#include <Pdh.h>

#if __clang__
#pragma clang diagnostic pop
#endif

typedef PDH_STATUS(__stdcall *fn_PdhOpenQueryW)(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH_HQUERY *phQuery);
typedef PDH_STATUS(__stdcall *fn_PdhAddEnglishCounterW)(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER *phCounter);
typedef PDH_STATUS(__stdcall *fn_PdhCollectQueryData)(PDH_HQUERY hQuery);
typedef PDH_STATUS(__stdcall *fn_PdhGetFormattedCounterValue)(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue);
typedef PDH_STATUS(__stdcall *fn_PdhCloseQuery)(PDH_HQUERY hQuery);

namespace L0 {
class KmdSysManager;

class WddmMemoryImp : public OsMemory, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;
    bool isMemoryModuleSupported() override;
    WddmMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    WddmMemoryImp() = default;
    ~WddmMemoryImp();

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    Device *pDevice = nullptr;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;

    fn_PdhOpenQueryW pdhOpenQuery = nullptr;
    fn_PdhAddEnglishCounterW pdhAddEnglishCounterW = nullptr;
    fn_PdhCollectQueryData pdhCollectQueryData = nullptr;
    fn_PdhGetFormattedCounterValue pdhGetFormattedCounterValue = nullptr;
    fn_PdhCloseQuery pdhCloseQuery = nullptr;

    bool pdhInitialized = false;
    bool pdhCounterAdded = false;
    PDH_HQUERY gpuQuery = nullptr;
    PDH_HCOUNTER usage = nullptr;
    HINSTANCE hGetProcPDH = nullptr;
};

} // namespace L0
