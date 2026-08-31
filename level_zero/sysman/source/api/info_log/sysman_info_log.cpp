/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

InfoLogHandleContext::InfoLogHandleContext() {
    supportedFormats = OsInfoLog::getSupportedInfoLogFormats();
}

void InfoLogHandleContext::destroyAllInstances() {
    for (auto &pInfoLog : handleList) {
        pInfoLog->destroyAllInstances();
    }
}

void InfoLogHandleContext::releaseInfoLogHandles() {
    handleList.clear();
}

InfoLogHandleContext::~InfoLogHandleContext() {
    destroyAllInstances();
    releaseInfoLogHandles();
}

void InfoLogHandleContext::createHandle(zes_intel_info_log_format_exp_t format) {
    std::unique_ptr<InfoLog> pInfoLog = std::make_unique<InfoLogImp>(format);
    handleList.push_back(std::move(pInfoLog));
}

void InfoLogHandleContext::init() {
    for (const auto &format : supportedFormats) {
        createHandle(format);
    }
}

ze_result_t InfoLogHandleContext::infoLogGet(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    std::call_once(initInfoLogOnce, [this]() {
        this->init();
        this->infoLogInitDone = true;
    });

    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);

    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }

    if (nullptr != phInfoLogs) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phInfoLogs[i] = handleList[i]->toHandle();
        }
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
