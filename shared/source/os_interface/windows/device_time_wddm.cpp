/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/device_time_wddm.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <memory>

#undef WIN32_NO_STATUS

namespace NEO {

bool DeviceTimeWddm::runEscape(Wddm *wddm, TimeStampDataHeader &escapeInfo) {
    if (wddm) {
        D3DKMT_ESCAPE escapeCommand = {0};

        GTDIGetGpuCpuTimestampsIn in = {GTDI_FNC_GET_GPU_CPU_TIMESTAMPS};
        uint32_t outSize = sizeof(GTDIGetGpuCpuTimestampsOut);

        escapeInfo.m_Header.EscapeCode = static_cast<decltype(escapeInfo.m_Header.EscapeCode)>(GFX_ESCAPE_IGPA_INSTRUMENTATION_CONTROL);
        escapeInfo.m_Header.Size = outSize;
        escapeInfo.m_Data.m_In = in;

        escapeCommand.Flags.Value = 0;
        escapeCommand.hAdapter = (D3DKMT_HANDLE)0;
        escapeCommand.hContext = (D3DKMT_HANDLE)0;
        escapeCommand.hDevice = (D3DKMT_HANDLE)wddm->getDeviceHandle();
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

double DeviceTimeWddm::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    double retVal = 0u;
    if (wddm) {
        retVal = 1000000000.0 / static_cast<double>(wddm->getTimestampFrequency());
    }

    return retVal;
}

uint64_t DeviceTimeWddm::getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
    uint64_t retVal = 0u;
    if (wddm) {
        retVal = static_cast<uint64_t>(wddm->getTimestampFrequency());
    }

    return retVal;
}

} // namespace NEO
