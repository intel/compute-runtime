/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/device_time_wddm.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <memory>

#undef WIN32_NO_STATUS

namespace NEO {

bool DeviceTimeWddm::runEscape(Wddm *wddm, TimeStampDataHeader &escapeInfo) {
    if (wddm) {
        D3DKMT_ESCAPE escapeCommand = {};

        GetGpuCpuTimestampsIn in{};
        uint32_t outSize = sizeof(GetGpuCpuTimestampsOut);

        escapeInfo.header.EscapeCode = static_cast<decltype(escapeInfo.header.EscapeCode)>(GFX_ESCAPE_IGPA_INSTRUMENTATION_CONTROL);
        escapeInfo.header.Size = outSize;
        escapeInfo.data.in = in;

        escapeCommand.Flags.Value = 0;
        escapeCommand.hAdapter = 0;
        escapeCommand.hContext = 0;
        escapeCommand.hDevice = static_cast<D3DKMT_HANDLE>(wddm->getDeviceHandle());
        escapeCommand.pPrivateDriverData = &escapeInfo;
        escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
        escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

        auto status = wddm->escape(escapeCommand);

        if (status == STATUS_SUCCESS) {
            return true;
        }
    }

    return false;
}

DeviceTimeWddm::DeviceTimeWddm(Wddm *wddm) {
    this->wddm = wddm;
}

double DeviceTimeWddm::getDynamicDeviceTimerResolution() const {
    double retVal = 0u;
    if (wddm) {
        retVal = 1000000000.0 / static_cast<double>(wddm->getTimestampFrequency());
    }

    return retVal;
}

uint64_t DeviceTimeWddm::getDynamicDeviceTimerClock() const {
    uint64_t retVal = 0u;
    if (wddm) {
        retVal = static_cast<uint64_t>(wddm->getTimestampFrequency());
    }

    return retVal;
}

void DeviceTimeWddm::convertTimestampsFromOaToCsDomain(const GfxCoreHelper &gfxCoreHelper, uint64_t &timestampData, uint64_t freqOA, uint64_t freqCS) {

    if (gfxCoreHelper.isTimestampShiftRequired() && freqCS > 0 && freqOA > 0) {
        auto freqRatio = static_cast<double>(freqOA) / static_cast<double>(freqCS);
        timestampData = static_cast<uint64_t>(timestampData / freqRatio);
    }
};
} // namespace NEO
