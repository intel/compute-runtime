/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"

namespace aub_stream {
class AubManager;
}

namespace NEO {

class DeviceTimeTbx : public DeviceTime {
  public:
    DeviceTimeTbx(aub_stream::AubManager &aubManager, uint32_t timestampMmioOffset);
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    bool isTimestampsRefreshEnabled() const override;
    bool isTimestampMmioReadAvailable() const override { return true; }
    uint32_t readLowDword() const;
    uint32_t readHighDword() const;

  protected:
    aub_stream::AubManager &aubManager;
    const uint32_t lowDwordOffset;
    const uint32_t highDwordOffset;
};

class OSTimeTbx : public OSTime {
  public:
    static std::unique_ptr<OSTime> create(aub_stream::AubManager &aubManager, std::optional<uint32_t> timestampMmioOffset);

    using OSTime::OSTime;
    bool getCpuTime(uint64_t *timeStamp) override;
};

} // namespace NEO
