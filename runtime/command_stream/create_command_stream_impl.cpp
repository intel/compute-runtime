/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

CommandStreamReceiver *createCommandStreamImpl(const HardwareInfo *pHwInfo) {
    auto funcCreate = commandStreamReceiverFactory[pHwInfo->pPlatform->eRenderCoreFamily];
    if (funcCreate == nullptr) {
        return nullptr;
    }
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr) {
        switch (csr) {
        case CSR_AUB:
            Gmm::initContext(pHwInfo->pPlatform,
                             pHwInfo->pSkuTable,
                             pHwInfo->pWaTable,
                             pHwInfo->pSysInfo);
            commandStreamReceiver = AUBCommandStreamReceiver::create(*pHwInfo, "aubfile", true);
            break;
        case CSR_TBX:
            Gmm::initContext(pHwInfo->pPlatform,
                             pHwInfo->pSkuTable,
                             pHwInfo->pWaTable,
                             pHwInfo->pSysInfo);
            commandStreamReceiver = TbxCommandStreamReceiver::create(*pHwInfo, false);
            break;
        case CSR_HW_WITH_AUB:
            commandStreamReceiver = funcCreate(*pHwInfo, true);
            break;
        case CSR_TBX_WITH_AUB:
            Gmm::initContext(pHwInfo->pPlatform,
                             pHwInfo->pSkuTable,
                             pHwInfo->pWaTable,
                             pHwInfo->pSysInfo);
            commandStreamReceiver = TbxCommandStreamReceiver::create(*pHwInfo, true);
            break;
        default:
            break;
        }
    } else {
        commandStreamReceiver = funcCreate(*pHwInfo, false);
    }
    return commandStreamReceiver;
}

bool getDevicesImpl(HardwareInfo **hwInfo, size_t &numDevicesReturned) {
    bool result;
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr) {
        switch (csr) {
        case CSR_AUB:
        case CSR_TBX: {
        case CSR_TBX_WITH_AUB:
            auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
            auto hwInfoConst = *platformDevices;
            getHwInfoForPlatformString(productFamily.c_str(), hwInfoConst);
            *hwInfo = const_cast<HardwareInfo *>(hwInfoConst);
            hardwareInfoSetupGt[hwInfoConst->pPlatform->eProductFamily](const_cast<GT_SYSTEM_INFO *>(hwInfo[0]->pSysInfo));
            numDevicesReturned = 1;
            return true;
        }
        case CSR_HW_WITH_AUB:
            return DeviceFactory::getDevices(hwInfo, numDevicesReturned);
        default:
            return false;
        }
    }
    result = DeviceFactory::getDevices(hwInfo, numDevicesReturned);
    DEBUG_BREAK_IF(result && (hwInfo == nullptr));
    return result;
}

} // namespace OCLRT
