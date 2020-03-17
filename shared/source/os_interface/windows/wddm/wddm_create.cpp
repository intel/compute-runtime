/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
Wddm *Wddm::createWddm(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    return new Wddm(std::move(hwDeviceId), rootDeviceEnvironment);
}

namespace ResLog {
fopenFuncPtr fopenPtr = &fopen;
vfprintfFuncPtr vfprintfPtr = &vfprintf;
fcloseFuncPtr fclosePtr = &fclose;
} // namespace ResLog

} // namespace NEO
