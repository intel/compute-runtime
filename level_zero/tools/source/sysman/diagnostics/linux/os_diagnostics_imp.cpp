/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {
const std::string LinuxDiagnosticsImp::deviceDir("device");

void LinuxDiagnosticsImp::osGetDiagProperties(zes_diag_properties_t *pProperties) {
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->haveTests = 0; // osGetDiagTests is Unsupported
    strncpy_s(pProperties->name, ZES_STRING_PROPERTY_SIZE, osDiagType.c_str(), osDiagType.size());
    return;
}

ze_result_t LinuxDiagnosticsImp::osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxDiagnosticsImp::osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) {
    return osRunDiagTestsinFW(pResult);
}

LinuxDiagnosticsImp::LinuxDiagnosticsImp(OsSysman *pOsSysman, const std::string &diagTests, ze_bool_t onSubdevice, uint32_t subdeviceId) : osDiagType(diagTests), isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    pProcfsAccess = &pLinuxSysmanImp->getProcfsAccess();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    auto device = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
    rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();
}

std::unique_ptr<OsDiagnostics> OsDiagnostics::create(OsSysman *pOsSysman, const std::string &diagTests, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxDiagnosticsImp> pLinuxDiagnosticsImp = std::make_unique<LinuxDiagnosticsImp>(pOsSysman, diagTests, onSubdevice, subdeviceId);
    return pLinuxDiagnosticsImp;
}

} // namespace L0
