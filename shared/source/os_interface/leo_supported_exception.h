/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <exception>

namespace NEO {

// LEO (Level Zero Executing OpenCL) detection mechanism.
//
// The device initialization path (DeviceFactory::createDevices) is shared between the legacy OpenCL
// driver (igdrcl) and the Level Zero driver (ze_intel_gpu). In EnableLEO auto-detect mode (EnableLEO
// == -1) the legacy OpenCL clGetPlatformIDs starts normal native initialization and, as soon as the
// hardware info and product helper are available, checks ProductHelper::isLEOSupported():
//   - if LEO is not supported, native initialization continues and OpenCL is serviced natively;
//   - if LEO is supported, this exception is thrown to quit initialization before the OpenCL stack is
//     fully brought up. Because the init code is shared, the throw is guarded to the legacy OpenCL API
//     (ApiSpecificConfig::getApiType() == OCL) so Level Zero's own initialization is never affected.
// clGetPlatformIDs catches the exception, records the decision and forwards the OpenCL entry points to
// the Level Zero backend. EnableLEO == 0 forces native OpenCL; EnableLEO == 1 forwards unconditionally
// without running native initialization.
//
// The detection is additionally gated by isLeoDetectionEnabled(): the production build always enables it,
// while unit tests read it from UltHwConfig (disabled by default) so only tests that exercise LEO opt in.
struct LeoSupportedException : std::exception {
    const char *what() const noexcept override {
        return "LEO is supported - aborting native OpenCL initialization to forward to Level Zero";
    }
};

} // namespace NEO
