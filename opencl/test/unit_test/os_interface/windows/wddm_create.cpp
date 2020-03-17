/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_logger_functions.h"

namespace NEO {
Wddm *Wddm::createWddm(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    return new WddmMock(rootDeviceEnvironment);
}

namespace ResLog {
fopenFuncPtr fopenPtr = &mockFopen;
vfprintfFuncPtr vfprintfPtr = &mockVfptrinf;
fcloseFuncPtr fclosePtr = &mockFclose;

uint32_t mockFopenCalled = 0;
uint32_t mockVfptrinfCalled = 0;
uint32_t mockFcloseCalled = 0;
} // namespace ResLog
} // namespace NEO
