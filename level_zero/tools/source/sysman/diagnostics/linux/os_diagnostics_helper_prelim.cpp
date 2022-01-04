/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

#include <linux/pci_regs.h>
namespace L0 {
//All memory mappings where LMEMBAR is being referenced are invalidated.
//Also prevents new ones from being created.
//It will invalidate LMEM memory mappings only when sysfs entry quiesce_gpu is set.

//the sysfs node will be at /sys/class/drm/card<n>/invalidate_lmem_mmaps
const std::string LinuxDiagnosticsImp::invalidateLmemFile("invalidate_lmem_mmaps");
// the sysfs node will be at /sys/class/drm/card<n>/quiesce_gpu
const std::string LinuxDiagnosticsImp::quiescentGpuFile("quiesce_gpu");
void OsDiagnostics::getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    if (IGFX_PVC == pLinuxSysmanImp->getProductFamily()) {
        FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
        if (pFwInterface != nullptr) {
            if (ZE_RESULT_SUCCESS == static_cast<FirmwareUtil *>(pFwInterface)->fwDeviceInit()) {
                static_cast<FirmwareUtil *>(pFwInterface)->fwSupportedDiagTests(supportedDiagTests);
            }
        }
    }
}

ze_result_t LinuxDiagnosticsImp::osRunDiagTestsinFW(zes_diag_result_t *pResult) {
    const int intVal = 1;
    // before running diagnostics need to close all active workloads
    // writing 1 to /sys/class/drm/card<n>/quiesce_gpu will signal KMD
    //GPU (every gt in the card) will be wedged.
    // GPU will only be unwedged after warm/cold reset
    ::pid_t myPid = pProcfsAccess->myProcessId();
    std::vector<::pid_t> processes;
    ze_result_t result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&pid : processes) {
        std::vector<int> fds;
        pLinuxSysmanImp->getPidFdsForOpenDevice(pProcfsAccess, pSysfsAccess, pid, fds);
        if (pid == myPid) {
            // L0 is expected to have this file open.
            // Keep list of fds. Close before unbind.
            continue;
        }
        if (!fds.empty()) {
            pProcfsAccess->kill(pid);
        }
    }
    result = pSysfsAccess->write(quiescentGpuFile, intVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    result = pSysfsAccess->write(invalidateLmemFile, intVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pFwInterface->fwRunDiagTests(osDiagType, pResult);
    pLinuxSysmanImp->diagnosticsReset = true;
    if (*pResult == ZES_DIAG_RESULT_REBOOT_FOR_REPAIR) {
        return pLinuxSysmanImp->osColdReset();
    }
    return pLinuxSysmanImp->osWarmReset(); // we need to at least do a Warm reset to bring the machine out of wedged state
}

} // namespace L0
