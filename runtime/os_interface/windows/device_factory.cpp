/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef _WIN32

#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/windows/wddm.h"

namespace OCLRT {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfos = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices) {
    HardwareInfo *tempHwInfos = new HardwareInfo[1];
    unsigned int devNum = 0;
    numDevices = 0;

    if (Wddm::enumAdapters(devNum, tempHwInfos[devNum])) {
        // Overwrite dynamic parameters
        tempHwInfos[devNum].capabilityTable.ftrSvm = tempHwInfos[devNum].pSkuTable->ftrSVM;

        HwHelper &hwHelper = HwHelper::get(tempHwInfos[devNum].pPlatform->eRenderCoreFamily);

        hwHelper.adjustDefaultEngineType(&tempHwInfos[devNum]);
        tempHwInfos[devNum].capabilityTable.defaultEngineType = DebugManager.flags.NodeOrdinal.get() == -1
                                                                    ? tempHwInfos[devNum].capabilityTable.defaultEngineType
                                                                    : static_cast<EngineType>(DebugManager.flags.NodeOrdinal.get());

        hwHelper.setCapabilityCoherencyFlag(&tempHwInfos[devNum], tempHwInfos[devNum].capabilityTable.ftrSupportsCoherency);

        hwHelper.setupPreemptionRegisters(&tempHwInfos[devNum], !!tempHwInfos[devNum].pWaTable->waEnablePreemptionGranularityControlByUMD);
        PreemptionHelper::adjustDefaultPreemptionMode(tempHwInfos[devNum].capabilityTable,
                                                      static_cast<bool>(tempHwInfos[devNum].pSkuTable->ftrGpGpuMidThreadLevelPreempt),
                                                      static_cast<bool>(tempHwInfos[devNum].pSkuTable->ftrGpGpuThreadGroupLevelPreempt),
                                                      static_cast<bool>(tempHwInfos[devNum].pSkuTable->ftrGpGpuMidBatchPreempt));
        tempHwInfos->capabilityTable.requiredPreemptionSurfaceSize = tempHwInfos->pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;

        // Instrumentation
        tempHwInfos[devNum].capabilityTable.instrumentationEnabled &= haveInstrumentation;

        tempHwInfos[devNum].capabilityTable.enableKmdNotify = DebugManager.flags.OverrideEnableKmdNotify.get() >= 0
                                                                  ? !!DebugManager.flags.OverrideEnableKmdNotify.get()
                                                                  : tempHwInfos[devNum].capabilityTable.enableKmdNotify;

        tempHwInfos[devNum].capabilityTable.delayKmdNotifyMicroseconds = DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get() >= 0
                                                                             ? static_cast<int64_t>(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get())
                                                                             : tempHwInfos[devNum].capabilityTable.delayKmdNotifyMicroseconds;

        numDevices = 1;
        *pHWInfos = tempHwInfos;
        DeviceFactory::numDevices = 1;
        DeviceFactory::hwInfos = tempHwInfos;

        return true;
    } else {
        delete[] tempHwInfos;
    }

    return false;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        for (unsigned int i = 0; i < DeviceFactory::numDevices; ++i) {
            delete hwInfos[i].pPlatform;
            delete hwInfos[i].pSkuTable;
            delete hwInfos[i].pWaTable;
            delete hwInfos[i].pSysInfo;
        }
        delete[] hwInfos;
    }
    DeviceFactory::hwInfos = nullptr;
    DeviceFactory::numDevices = 0;
}

void Device::appendOSExtensions(std::string &deviceExtensions) {
    deviceExtensions += "cl_intel_simultaneous_sharing ";

    simultaneousInterops = {CL_GL_CONTEXT_KHR,
                            CL_WGL_HDC_KHR,
                            CL_CONTEXT_ADAPTER_D3D9_KHR,
                            CL_CONTEXT_D3D9_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_D3D9EX_KHR,
                            CL_CONTEXT_D3D9EX_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_DXVA_KHR,
                            CL_CONTEXT_DXVA_DEVICE_INTEL,
                            CL_CONTEXT_D3D10_DEVICE_KHR,
                            CL_CONTEXT_D3D11_DEVICE_KHR,
                            0};
}
} // namespace OCLRT

#endif
