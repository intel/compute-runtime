/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"

namespace NEO {
struct MockWddmResidencyLogger : public WddmResidencyLogger {
    using WddmResidencyLogger::endTime;
    using WddmResidencyLogger::enterWait;
    using WddmResidencyLogger::makeResidentCall;
    using WddmResidencyLogger::pagingLog;
    using WddmResidencyLogger::pendingMakeResident;
    using WddmResidencyLogger::pendingTime;
    using WddmResidencyLogger::waitStartTime;
    using WddmResidencyLogger::WddmResidencyLogger;
};
} // namespace NEO
