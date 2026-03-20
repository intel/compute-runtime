/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Minimal DrmNlApi definition for builds without netlink headers available.
// Only to be included by sysman_ras_util_netlink_stub.cpp.
// When netlink headers are present, include sysman_drm_nl_api.h instead.

#pragma once

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"
#include <level_zero/zes_api.h>

#include <vector>

namespace L0 {
namespace Sysman {

class DrmNlApi {
  public:
    DrmNlApi() = default;
    virtual ~DrmNlApi() = default;

    virtual ze_result_t listNodes(std::vector<DrmRasNode> &nodeList) { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    virtual ze_result_t getErrorCounter(const uint32_t &nodeId, const uint32_t &errorId, DrmErrorCounter &errorCounter) { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    virtual ze_result_t getErrorsList(const uint32_t &nodeId, std::vector<DrmErrorCounter> &errorList) { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
};

} // namespace Sysman
} // namespace L0
