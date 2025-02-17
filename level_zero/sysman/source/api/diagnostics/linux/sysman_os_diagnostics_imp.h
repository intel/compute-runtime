/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics_imp.h"
#include "level_zero/sysman/source/api/diagnostics/sysman_os_diagnostics.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class FirmwareUtil;
class SysFsAccessInterface;

class LinuxDiagnosticsImp : public OsDiagnostics, NEO::NonCopyableAndNonMovableClass {
  public:
    void osGetDiagProperties(zes_diag_properties_t *pProperties) override;
    ze_result_t osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) override;
    ze_result_t osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) override;
    ze_result_t osRunDiagTestsinFW(zes_diag_result_t *pResult);
    LinuxDiagnosticsImp() = default;
    LinuxDiagnosticsImp(OsSysman *pOsSysman, const std::string &diagTests);
    ~LinuxDiagnosticsImp() override = default;
    std::string osDiagType = "unknown";

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    FirmwareUtil *pFwInterface = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    ze_result_t waitForQuiescentCompletion();

  private:
    static const std::string quiescentGpuFile;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    static const std::string invalidateLmemFile;
    static const std::string deviceDir;
};

} // namespace Sysman
} // namespace L0
