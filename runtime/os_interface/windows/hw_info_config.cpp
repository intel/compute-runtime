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

#include "runtime/command_stream/preemption.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/hw_helper.h"
#include "instrumentation.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {

HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT] = {};

int HwInfoConfig::configureHwInfo(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    HwHelper &hwHelper = HwHelper::get(outHwInfo->pPlatform->eRenderCoreFamily);

    outHwInfo->capabilityTable.ftrSvm = outHwInfo->pSkuTable->ftrSVM;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    const auto nodeOrdinal = DebugManager.flags.NodeOrdinal.get();
    outHwInfo->capabilityTable.defaultEngineType = nodeOrdinal == -1
                                                       ? outHwInfo->capabilityTable.defaultEngineType
                                                       : static_cast<EngineType>(nodeOrdinal);

    hwHelper.setCapabilityCoherencyFlag(outHwInfo, outHwInfo->capabilityTable.ftrSupportsCoherency);

    hwHelper.setupPreemptionRegisters(outHwInfo, outHwInfo->pWaTable->waEnablePreemptionGranularityControlByUMD);
    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuMidThreadLevelPreempt),
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuThreadGroupLevelPreempt),
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuMidBatchPreempt));
    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;

    outHwInfo->capabilityTable.instrumentationEnabled &= haveInstrumentation;

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);

    // Product specific config
    int ret = configureHardwareCustom(outHwInfo, osIface);
    if (ret != 0) {
        outHwInfo = {};
    }
    return ret;
}

} // namespace OCLRT
