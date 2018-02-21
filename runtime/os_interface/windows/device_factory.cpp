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
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/windows/wddm.h"
#include "runtime/device/device.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/sku_info/operations/sku_info_receiver.h"

namespace OCLRT {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfos = nullptr;
void *DeviceFactory::internal = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices) {
    bool success = false;
    HardwareInfo *tempHwInfos = new HardwareInfo[1];
    ADAPTER_INFO *adapterInfo = new ADAPTER_INFO[1];
    unsigned int devNum = 0;
    numDevices = 0;

    success = Wddm::enumAdapters(devNum, adapterInfo);

    if (success) {
        auto featureTable = new FeatureTable();
        auto waTable = new WorkaroundTable();
        tempHwInfos[devNum].pPlatform = new PLATFORM(adapterInfo->GfxPlatform);
        tempHwInfos[devNum].pSkuTable = featureTable;
        tempHwInfos[devNum].pWaTable = waTable;
        tempHwInfos[devNum].pSysInfo = new GT_SYSTEM_INFO(adapterInfo->SystemInfo);

        SkuInfoReceiver::receiveFtrTableFromAdapterInfo(featureTable, adapterInfo);
        SkuInfoReceiver::receiveWaTableFromAdapterInfo(waTable, adapterInfo);

        auto productFamily = tempHwInfos[devNum].pPlatform->eProductFamily;
        DEBUG_BREAK_IF(hardwareInfoTable[productFamily] == nullptr);

        tempHwInfos[devNum].capabilityTable = hardwareInfoTable[productFamily]->capabilityTable;

        // Overwrite dynamic parameters
        tempHwInfos[devNum].capabilityTable.maxRenderFrequency = adapterInfo->MaxRenderFreq;
        tempHwInfos[devNum].capabilityTable.ftrSvm = adapterInfo->SkuTable.FtrSVM;

        HwHelper &hwHelper = HwHelper::get(adapterInfo->GfxPlatform.eRenderCoreFamily);

        hwHelper.adjustDefaultEngineType(&tempHwInfos[devNum]);
        tempHwInfos[devNum].capabilityTable.defaultEngineType = DebugManager.flags.NodeOrdinal.get() == -1
                                                                    ? tempHwInfos[devNum].capabilityTable.defaultEngineType
                                                                    : static_cast<EngineType>(DebugManager.flags.NodeOrdinal.get());

        hwHelper.setCapabilityCoherencyFlag(&tempHwInfos[devNum], tempHwInfos[devNum].capabilityTable.ftrSupportsCoherency);

        hwHelper.setupPreemptionRegisters(&tempHwInfos[devNum], !!adapterInfo->WaTable.WaEnablePreemptionGranularityControlByUMD);
        PreemptionHelper::adjustDefaultPreemptionMode(tempHwInfos[devNum].capabilityTable,
                                                      static_cast<bool>(adapterInfo->SkuTable.FtrGpGpuMidThreadLevelPreempt),
                                                      static_cast<bool>(adapterInfo->SkuTable.FtrGpGpuThreadGroupLevelPreempt),
                                                      static_cast<bool>(adapterInfo->SkuTable.FtrGpGpuMidBatchPreempt));

        // Instrumentation
        tempHwInfos[devNum].capabilityTable.instrumentationEnabled &= haveInstrumentation && (adapterInfo->Caps.InstrumentationIsEnabled != 0);

        tempHwInfos[devNum].capabilityTable.enableKmdNotify = DebugManager.flags.OverrideEnableKmdNotify.get() >= 0
                                                                  ? !!DebugManager.flags.OverrideEnableKmdNotify.get()
                                                                  : tempHwInfos[devNum].capabilityTable.enableKmdNotify;

        tempHwInfos[devNum].capabilityTable.delayKmdNotifyMs = DebugManager.flags.OverrideKmdNotifyDelayMs.get() >= 0
                                                                   ? static_cast<int64_t>(DebugManager.flags.OverrideKmdNotifyDelayMs.get())
                                                                   : tempHwInfos[devNum].capabilityTable.delayKmdNotifyMs;

        numDevices = 1;
        *pHWInfos = tempHwInfos;
        internal = static_cast<void *>(adapterInfo);
        DeviceFactory::numDevices = 1;
        DeviceFactory::hwInfos = tempHwInfos;
    } else {
        delete[] tempHwInfos;
    }

    return success;
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
        ADAPTER_INFO *adapterInfo = static_cast<ADAPTER_INFO *>(internal);
        delete[] adapterInfo;
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
