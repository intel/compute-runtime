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

#include "unit_tests/libult/mock_gfx_family.h"
#include "runtime/helpers/hw_helper.inl"

namespace OCLRT {

bool (*GENX::isSimulationFcn)(unsigned short) = nullptr;

template <>
size_t HwHelperHw<GENX>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <>
void HwHelperHw<GENX>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(pHwInfo->pPlatform);
    if (pPlatform->usDeviceID == 20) {
        coherencyFlag = false;
    } else {
        coherencyFlag = true;
    }
}

template <>
bool HwHelperHw<GENX>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    pHwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580 = enable;
    return enable;
}

struct hw_helper_static_init {
    hw_helper_static_init() {
        hwHelperFactory[IGFX_UNKNOWN_CORE] = &HwHelperHw<GENX>::get();
    }
};

template class HwHelperHw<GENX>;

hw_helper_static_init si;
} // namespace OCLRT
