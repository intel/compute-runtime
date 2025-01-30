/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"

namespace L0 {
const std::string LinuxDiagnosticsImp::deviceDir("device");

// the sysfs node will be at /sys/class/drm/card<n>/invalidate_lmem_mmaps
const std::string LinuxDiagnosticsImp::invalidateLmemFile("invalidate_lmem_mmaps");
// the sysfs node will be at /sys/class/drm/card<n>/quiesce_gpu
const std::string LinuxDiagnosticsImp::quiescentGpuFile("quiesce_gpu");
void OsDiagnostics::getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests) {}

// before running diagnostics need to close all active workloads
// writing 1 to /sys/class/drm/card<n>/quiesce_gpu will signal KMD
// to close and clear all allocations,
// ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE will be sent till the kworker confirms that
// all allocations are closed and GPU is be wedged.
// GPU will only be unwedged after warm/cold reset
// writing 1 to /sys/class/drm/card<n>/invalidate_lmem_mmaps clears
// all memory mappings where LMEMBAR is being referenced are invalidated.
// Also prevents new ones from being created.
// It will invalidate LMEM memory mappings only when sysfs entry quiesce_gpu is set.
ze_result_t LinuxDiagnosticsImp::waitForQuiescentCompletion() {
    uint32_t count = 0;
    const int intVal = 1;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    do {
        result = pSysfsAccess->write(quiescentGpuFile, intVal);
        if (ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE == result) {
            count++;
            NEO::sleep(std::chrono::seconds(1)); // Sleep for 1second every loop, gives enough time for KMD to clear all allocations and wedge the system
            auto processResult = pLinuxSysmanImp->gpuProcessCleanup(true);
            if (ZE_RESULT_SUCCESS != processResult) {
                return processResult;
            }
        } else if (ZE_RESULT_SUCCESS == result) {
            break;
        } else {
            return result;
        }
    } while (count < 10); // limiting to 10 retries as we can endup going into a infinite loop if the cleanup and a process start are out of sync
    result = pSysfsAccess->write(invalidateLmemFile, intVal);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, invalidateLmemFile.c_str(), result);
        return result;
    }
    return result;
}

ze_result_t LinuxDiagnosticsImp::osRunDiagTestsinFW(zes_diag_result_t *pResult) {
    pLinuxSysmanImp->diagnosticsReset = true;
    auto pDevice = pLinuxSysmanImp->getDeviceHandle();
    auto devicePtr = static_cast<DeviceImp *>(pDevice);
    NEO::ExecutionEnvironment *executionEnvironment = devicePtr->getNEODevice()->getExecutionEnvironment();
    auto restorer = std::make_unique<L0::ExecutionEnvironmentRefCountRestore>(executionEnvironment);
    pLinuxSysmanImp->releaseDeviceResources();
    ze_result_t result = pLinuxSysmanImp->gpuProcessCleanup(true);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): gpuProcessCleanup() failed and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = waitForQuiescentCompletion();
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): waitForQuiescentCompletion() failed and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    result = pFwInterface->fwRunDiagTests(osDiagType, pResult);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): fwRunDiagTests() failed and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    if (osDiagType == "MEMORY_PPR") {
        pLinuxSysmanImp->isMemoryDiagnostics = true;
    }

    if (*pResult == ZES_DIAG_RESULT_REBOOT_FOR_REPAIR) {
        result = pLinuxSysmanImp->osColdReset();
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): osColdReset() failed and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    } else {
        result = pLinuxSysmanImp->osWarmReset(); // we need to at least do a Warm reset to bring the machine out of wedged state
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): osWarmReset() failed and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    }
    return pLinuxSysmanImp->initDevice();
}

void LinuxDiagnosticsImp::osGetDiagProperties(zes_diag_properties_t *pProperties) {
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->haveTests = 0; // osGetDiagTests is Unsupported
    strncpy_s(pProperties->name, ZES_STRING_PROPERTY_SIZE, osDiagType.c_str(), osDiagType.size());
    return;
}

ze_result_t LinuxDiagnosticsImp::osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxDiagnosticsImp::osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) {
    return osRunDiagTestsinFW(pResult);
}

LinuxDiagnosticsImp::LinuxDiagnosticsImp(OsSysman *pOsSysman, const std::string &diagTests) : osDiagType(diagTests) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

std::unique_ptr<OsDiagnostics> OsDiagnostics::create(OsSysman *pOsSysman, const std::string &diagTests) {
    std::unique_ptr<LinuxDiagnosticsImp> pLinuxDiagnosticsImp = std::make_unique<LinuxDiagnosticsImp>(pOsSysman, diagTests);
    return pLinuxDiagnosticsImp;
}

} // namespace L0
