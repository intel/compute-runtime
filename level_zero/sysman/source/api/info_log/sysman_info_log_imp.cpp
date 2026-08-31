/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

ze_result_t InfoLogImp::infoLogGetProperties(zes_intel_info_log_properties_exp_t *pProperties) {
    if (initResult != ZE_RESULT_SUCCESS) {
        return initResult;
    }

    // Only the [out] members are assigned; the caller's stype and pNext extension chain survive.
    pProperties->infoLogType = infoLogProperties.infoLogType;
    pProperties->infoLogFormat = infoLogProperties.infoLogFormat;
    pProperties->isNamedInstancedCollectionSupported = infoLogProperties.isNamedInstancedCollectionSupported;
    pProperties->isPeekSupported = infoLogProperties.isPeekSupported;

    return ZE_RESULT_SUCCESS;
}

ze_result_t InfoLogImp::infoLogCreateInstance(const char *pInstanceName,
                                              zes_intel_info_log_instance_exp_desc_t *pDesc,
                                              zes_intel_info_log_instance_handle_t *phInfoLogInstance) {
    if (initResult != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Info log properties were not captured, returning error: 0x%x\n", NEO_FUNCTION_NAME, initResult);
        return initResult;
    }

    if (pInstanceName != nullptr && !infoLogProperties.isNamedInstancedCollectionSupported) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Named collection instances are not supported\n", NEO_FUNCTION_NAME);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::lock_guard<std::mutex> lock(instancesMutex);

    if (pInstanceName != nullptr && activeInstanceNames.count(pInstanceName) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Collection instance '%s' is already in use\n", NEO_FUNCTION_NAME, pInstanceName);
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    std::unique_ptr<OsInfoLogInstance> pOsInstance;
    auto result = pOsInfoLog->createInstance(pInstanceName, pDesc, pOsInstance);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto pInstance = std::make_unique<InfoLogInstanceImp>(this, pInstanceName, std::move(pOsInstance));
    if (pInstanceName != nullptr) {
        activeInstanceNames.insert(pInstanceName);
    }

    *phInfoLogInstance = pInstance->toHandle();
    instances.push_back(std::move(pInstance));

    return ZE_RESULT_SUCCESS;
}

ze_result_t InfoLogImp::destroyInstance(InfoLogInstance *pInstance) {
    std::lock_guard<std::mutex> lock(instancesMutex);

    auto it = std::find_if(instances.begin(), instances.end(),
                           [pInstance](const std::unique_ptr<InfoLogInstance> &pOwned) { return pOwned.get() == pInstance; });
    if (it == instances.end()) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto result = pInstance->teardown();

    auto pInstanceImp = static_cast<InfoLogInstanceImp *>(pInstance);
    if (pInstanceImp->isNamed()) {
        activeInstanceNames.erase(pInstanceImp->getInstanceName());
    }

    instances.erase(it);

    return result;
}

void InfoLogImp::destroyAllInstances() {
    std::lock_guard<std::mutex> lock(instancesMutex);

    for (auto &pInstance : instances) {
        pInstance->teardown();
    }

    instances.clear();
    activeInstanceNames.clear();
}

void InfoLogImp::init() {
    initResult = pOsInfoLog->getProperties(&infoLogProperties);
}

InfoLogImp::InfoLogImp(zes_intel_info_log_format_exp_t format) {
    pOsInfoLog = OsInfoLog::create(format);
    init();
}

// The call is qualified because virtual dispatch is already restricted to this class once the
// destructor runs.
InfoLogImp::~InfoLogImp() {
    InfoLogImp::destroyAllInstances();
}

} // namespace Sysman
} // namespace L0
