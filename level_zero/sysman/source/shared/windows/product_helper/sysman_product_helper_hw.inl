/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;
    request.requestId = KmdSysman::Requests::Temperature::CurrentTemperature;

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainPackage;
        break;
    case ZES_TEMP_SENSORS_GPU:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainDGPU;
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainHBM;
        break;
    default:
        *pTemperature = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pTemperature = static_cast<double>(value);

    return status;
}

} // namespace Sysman
} // namespace L0
