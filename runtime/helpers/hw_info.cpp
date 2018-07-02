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

#include "hw_info.h"
#include "hw_cmds.h"

namespace OCLRT {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *skuTable, const WorkaroundTable *waTable,
                           const GT_SYSTEM_INFO *sysInfo, const RuntimeCapabilityTable &capabilityTable)
    : pPlatform(platform), pSkuTable(skuTable), pWaTable(waTable), pSysInfo(sysInfo), capabilityTable(capabilityTable) {
}

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetupGt[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *) = {
    nullptr,
};

const FeatureTable emptySkuTable = {};
const WorkaroundTable emptyWaTable = {};

const char *getPlatformType(const HardwareInfo &hwInfo) {
    if (hwInfo.capabilityTable.isCore) {
        return "core";
    }
    return "lp";
}

bool getHwInfoForPlatformString(const char *str, const HardwareInfo *&hwInfoIn) {
    bool ret = false;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (hardwarePrefix[j] == nullptr)
            continue;
        if (strcmp(hardwarePrefix[j], str) == 0) {
            hwInfoIn = hardwareInfoTable[j];
            ret = true;
            break;
        }
    }
    return ret;
}
} // namespace OCLRT
