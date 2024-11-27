/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/sysman_const.h"

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

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isTempModuleSupported(zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) {
    if ((type == ZES_TEMP_SENSORS_GLOBAL_MIN) || (type == ZES_TEMP_SENSORS_GPU_MIN)) {
        return false;
    }

    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response = {};
    uint32_t value = 0;

    request.paramInfo = static_cast<uint32_t>(type);
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;
    request.requestId = KmdSysman::Requests::Temperature::NumTemperatureDomains;

    status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    }

    if (value == 0) {
        return false;
    }

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;
    request.requestId = KmdSysman::Requests::Temperature::CurrentTemperature;

    return (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS);
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, WddmSysmanImp *pWddmSysmanImp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = false;
    properties->havePacketCounters = false;
    properties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandWidth(zes_mem_bandwidth_t *pBandwidth, WddmSysmanImp *pWddmSysmanImp) {
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    uint32_t retValu32 = 0;
    uint64_t retValu64 = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;

    request.requestId = KmdSysman::Requests::Memory::MaxBandwidth;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::CurrentBandwidthRead;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::CurrentBandwidthWrite;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    pBandwidth->maxBandwidth = 0;
    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu32, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        pBandwidth->maxBandwidth = static_cast<uint64_t>(retValu32) * static_cast<uint64_t>(mbpsToBytesPerSecond);
    }

    pBandwidth->readCounter = 0;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu64, sizeof(uint64_t), vResponses[1].dataBuffer, sizeof(uint64_t));
        pBandwidth->readCounter = retValu64;
    }

    pBandwidth->writeCounter = 0;
    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu64, sizeof(uint64_t), vResponses[2].dataBuffer, sizeof(uint64_t));
        pBandwidth->writeCounter = retValu64;
    }

    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    pBandwidth->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();

    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<zes_power_domain_t> SysmanProductHelperHw<gfxProduct>::getNumberOfPowerDomainsSupported(WddmSysmanImp *pWddmSysmanImp) {
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::NumPowerDomains;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    std::vector<zes_power_domain_t> powerDomains;
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "No power domains are supported, power handles will not be created.\n");
        return powerDomains;
    }

    uint32_t supportedPowerDomains = 0;
    memcpy_s(&supportedPowerDomains, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    switch (supportedPowerDomains) {
    case 1:
        powerDomains.push_back(ZES_POWER_DOMAIN_PACKAGE);
        break;
    case 2:
        powerDomains.push_back(ZES_POWER_DOMAIN_PACKAGE);
        powerDomains.push_back(ZES_POWER_DOMAIN_CARD);
        break;
    default:
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Unexpected value returned by KMD, power handles will not be created.\n");
        break;
    }
    return powerDomains;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerProperties(zes_power_properties_t *pProperties, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) {
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.paramInfo = static_cast<uint32_t>(powerGroupToDomainTypeMap.at(powerDomain));
    request.requestId = KmdSysman::Requests::Power::EnergyThresholdSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::TdpDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::MinPowerLimitDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::MaxPowerLimitDefault;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->canControl, sizeof(ze_bool_t), vResponses[0].dataBuffer, sizeof(ze_bool_t));
        memcpy_s(&pProperties->isEnergyThresholdSupported, sizeof(ze_bool_t), vResponses[0].dataBuffer, sizeof(ze_bool_t));
    }

    pProperties->defaultLimit = -1;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->defaultLimit, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
    }

    pProperties->minLimit = -1;
    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->minLimit, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
    }

    pProperties->maxLimit = -1;
    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->maxLimit, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
    }

    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerPropertiesExt(zes_power_ext_properties_t *pExtPoperties, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) {
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    pExtPoperties->domain = powerDomain;
    if (pExtPoperties->defaultLimit) {
        KmdSysman::RequestProperty request;
        KmdSysman::ResponseProperty response;

        request.commandId = KmdSysman::Command::Get;
        request.componentId = KmdSysman::Component::PowerComponent;
        request.paramInfo = static_cast<uint32_t>(powerGroupToDomainTypeMap.at(powerDomain));
        request.requestId = KmdSysman::Requests::Power::TdpDefault;

        ze_result_t status = pKmdSysManager->requestSingle(request, response);
        pExtPoperties->defaultLimit->limit = -1;

        if (status == ZE_RESULT_SUCCESS) {
            memcpy_s(&pExtPoperties->defaultLimit->limit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        }

        pExtPoperties->defaultLimit->limitUnit = ZES_LIMIT_UNIT_POWER;
        pExtPoperties->defaultLimit->enabledStateLocked = true;
        pExtPoperties->defaultLimit->intervalValueLocked = true;
        pExtPoperties->defaultLimit->limitValueLocked = true;
        pExtPoperties->defaultLimit->source = ZES_POWER_SOURCE_ANY;
        pExtPoperties->defaultLimit->level = ZES_POWER_LEVEL_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) {
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    uint64_t energyCounter64Bit = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.paramInfo = static_cast<uint32_t>(powerGroupToDomainTypeMap.at(powerDomain));
    request.requestId = KmdSysman::Requests::Power::CurrentEnergyCounter64Bit;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        memcpy_s(&energyCounter64Bit, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
        pEnergy->energy = energyCounter64Bit;
        pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();
    }

    return status;
}

template <PRODUCT_FAMILY gfxProduct>
std::map<unsigned long, std::map<std::string, uint32_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return false;
}

} // namespace Sysman
} // namespace L0
