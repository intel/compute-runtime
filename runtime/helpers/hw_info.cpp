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

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetupGt[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *) = {
    nullptr,
};

const FeatureTable emptySkuTable = {};
const WorkaroundTable emptyWaTable = {};

const PLATFORM unknownPlatform = {
    IGFX_UNKNOWN,
    PCH_UNKNOWN,
    IGFX_UNKNOWN_CORE,
    IGFX_UNKNOWN_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const GT_SYSTEM_INFO unknownSysInfo = {};

const HardwareInfo unknownHardware = {
    &unknownPlatform,
    &emptySkuTable,
    &emptyWaTable,
    &unknownSysInfo,
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, PreemptionMode::Disabled, {false, false}, nullptr}};

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
