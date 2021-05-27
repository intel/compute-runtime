/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_context.h"

#include <vector>

namespace NEO {
class Drm;

class OsContextLinux : public OsContext {
  public:
    OsContextLinux() = delete;
    ~OsContextLinux() override;
    OsContextLinux(Drm &drm, uint32_t contextId, DeviceBitfield deviceBitfield,
                   EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice);

    unsigned int getEngineFlag() const { return engineFlag; }
    const std::vector<uint32_t> &getDrmContextIds() const { return drmContextIds; }
    const std::vector<uint32_t> &getDrmVmIds() const { return drmVmIds; }
    bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const override;
    Drm &getDrm() const;
    void waitForPagingFence();
    static OsContext *create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice);

  protected:
    void initializeContext() override;

    unsigned int engineFlag = 0;
    std::vector<uint32_t> drmContextIds;
    std::vector<uint32_t> drmVmIds;
    Drm &drm;
};
} // namespace NEO
