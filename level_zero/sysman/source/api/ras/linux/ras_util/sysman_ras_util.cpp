/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"

namespace L0 {
namespace Sysman {

std::unique_ptr<RasUtil> RasUtil::create(RasInterfaceType rasInterface, LinuxSysmanImp *pLinuxSysmanImp, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    switch (rasInterface) {
    case RasInterfaceType::pmu:
        return std::make_unique<PmuRasUtil>(type, pLinuxSysmanImp, onSubdevice, subdeviceId);
    case RasInterfaceType::gsc:
        return std::make_unique<GscRasUtil>(type, pLinuxSysmanImp, subdeviceId);
    default:
        return std::make_unique<RasUtilNone>();
    }
}

} // namespace Sysman
} // namespace L0
