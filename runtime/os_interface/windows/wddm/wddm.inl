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

#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {

template <typename GfxFamily>
bool Wddm::configureDeviceAddressSpace() {
    SYSTEM_INFO sysInfo;
    Wddm::getSystemInfo(&sysInfo);
    maximumApplicationAddress = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
    minAddress = windowsMinAddress;

    return gmmMemory->configureDeviceAddressSpace(adapter, device, gdi->escape,
                                                  maximumApplicationAddress + 1u,
                                                  0, 0, featureTable->ftrL3IACoherency, 0, 0);
}

template <typename GfxFamily>
bool Wddm::init() {
    if (gdi != nullptr && gdi->isInitialized() && !initialized) {
        if (!openAdapter()) {
            return false;
        }
        if (!queryAdapterInfo()) {
            return false;
        }
        if (!createDevice()) {
            return false;
        }
        if (!createPagingQueue()) {
            return false;
        }
        if (!initGmmContext()) {
            return false;
        }
        if (!configureDeviceAddressSpace<GfxFamily>()) {
            return false;
        }
        if (!createContext()) {
            return false;
        }
        if (!createMonitoredFence()) {
            return false;
        }
        initialized = true;
    }
    return initialized;
}
} // namespace OCLRT
