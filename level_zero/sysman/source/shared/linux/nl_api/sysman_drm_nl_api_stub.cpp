/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api_stub.h"

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

namespace L0 {
namespace Sysman {

DrmNlApi *LinuxSysmanDriverImp::createDrmNlApi() {
    return nullptr;
}

void LinuxSysmanDriverImp::destroyDrmNlApi(DrmNlApi *pDrmNl) {
    delete pDrmNl;
}

} // namespace Sysman
} // namespace L0
