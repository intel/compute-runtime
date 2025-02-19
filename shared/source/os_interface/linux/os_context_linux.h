/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/os_interface/os_context.h"

#include <array>
#include <atomic>
#include <vector>

namespace NEO {
class Drm;

class OsContextLinux : public OsContext {
  public:
    OsContextLinux() = delete;
    ~OsContextLinux() override;
    OsContextLinux(Drm &drm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);

    unsigned int getEngineFlag() const { return engineFlag; }
    void setEngineFlag(unsigned int engineFlag) { this->engineFlag = engineFlag; }
    const std::vector<uint32_t> &getDrmContextIds() const { return drmContextIds; }
    const std::vector<uint32_t> &getDrmVmIds() const { return drmVmIds; }
    bool isDirectSubmissionSupported() const override;
    Drm &getDrm() const;
    virtual std::pair<uint64_t, uint64_t> getFenceAddressAndValToWait(uint32_t vmHandleId, bool isLocked);
    virtual void waitForPagingFence();
    static OsContext *create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);
    void reInitializeContext() override;
    void setHangDetected() {
        contextHangDetected = true;
    }
    bool isHangDetected() const {
        return contextHangDetected;
    }

    uint64_t getOfflineDumpContextId(uint32_t deviceIndex) const override;

    uint64_t getNextFenceVal(uint32_t deviceIndex) { return fenceVal[deviceIndex] + 1; }
    void incFenceVal(uint32_t deviceIndex) { fenceVal[deviceIndex]++; }
    uint64_t *getFenceAddr(uint32_t deviceIndex) { return &pagingFence[deviceIndex]; }
    void waitForBind(uint32_t drmIterator);
    bool isDirectSubmissionLightActive() const override;

  protected:
    bool initializeContext(bool allocateInterrupt) override;
    void isOpenVinoLoaded();

    unsigned int engineFlag = 0;
    std::vector<uint32_t> drmContextIds;
    std::vector<uint32_t> drmVmIds;

    std::array<uint64_t, EngineLimits::maxHandleCount> pagingFence;
    std::array<uint64_t, EngineLimits::maxHandleCount> fenceVal;

    std::once_flag ovLoadedFlag{};
    Drm &drm;
    bool contextHangDetected = false;
    bool ovLoaded = false;
};
} // namespace NEO
