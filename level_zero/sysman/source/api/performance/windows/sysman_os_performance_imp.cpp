/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/performance/windows/sysman_os_performance_imp.h"

namespace L0 {
namespace Sysman {

constexpr double maxPerformanceFactor = 100;
constexpr double halfOfMaxPerformanceFactor = 50;
constexpr double minPerformanceFactor = 0;
constexpr uint32_t mediaDynamicMode = 0;
constexpr uint32_t mediaOneToTwoMode = 128;
constexpr uint32_t mediaOneToOneMode = 256;

ze_result_t WddmPerformanceImp::osPerformanceGetConfig(double *pFactor) {
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    uint32_t value = 0;
    request.paramInfo = static_cast<uint32_t>(KmdSysman::ActivityDomainsType::ActivityDomainMedia);
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PerformanceComponent;
    request.requestId = KmdSysman::Requests::Performance::Factor;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    if (value == mediaOneToOneMode) {
        *pFactor = maxPerformanceFactor;
    } else if (value == mediaOneToTwoMode) {
        *pFactor = halfOfMaxPerformanceFactor;
    } else if (value == mediaDynamicMode) {
        *pFactor = minPerformanceFactor;
    } else {
        status = ZE_RESULT_ERROR_UNKNOWN;
    }
    return status;
}

ze_result_t WddmPerformanceImp::osPerformanceSetConfig(double pFactor) {
    if (pFactor < minPerformanceFactor || pFactor > maxPerformanceFactor) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    uint32_t value = 0;
    request.paramInfo = static_cast<uint32_t>(KmdSysman::ActivityDomainsType::ActivityDomainMedia);
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PerformanceComponent;
    request.requestId = KmdSysman::Requests::Performance::Factor;
    request.dataSize = sizeof(uint32_t);
    if (pFactor > halfOfMaxPerformanceFactor) {
        value = mediaOneToOneMode;
    } else if (pFactor > minPerformanceFactor) {
        value = mediaOneToTwoMode;
    } else {
        value = mediaDynamicMode;
    }
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmPerformanceImp::osPerformanceGetProperties(zes_perf_properties_t &properties) {
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subDeviceId;
    properties.engines = domain;
    return ZE_RESULT_SUCCESS;
}

bool WddmPerformanceImp::isPerformanceSupported(void) {
    if (domain == ZES_ENGINE_TYPE_FLAG_MEDIA) {
        KmdSysman::RequestProperty request;
        KmdSysman::ResponseProperty response;
        uint32_t value = 0;

        request.paramInfo = static_cast<uint32_t>(KmdSysman::ActivityDomainsType::ActivityDomainMedia);
        request.commandId = KmdSysman::Command::Get;
        request.componentId = KmdSysman::Component::PerformanceComponent;
        request.requestId = KmdSysman::Requests::Performance::NumPerformanceDomains;

        ze_result_t status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return false;
        }

        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        return static_cast<bool>(value);
    }
    return false;
}

WddmPerformanceImp::WddmPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                       zes_engine_type_flag_t domain) : isSubdevice(onSubdevice), subDeviceId(subdeviceId), domain(domain) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsPerformance *OsPerformance::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId,
                                     zes_engine_type_flag_t domain) {
    WddmPerformanceImp *pWddmPerformanceImp = new WddmPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain);
    return static_cast<OsPerformance *>(pWddmPerformanceImp);
}

} // namespace Sysman
} // namespace L0
