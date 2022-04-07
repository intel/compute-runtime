/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include "igfxfmid.h"

#include <memory>
#include <vector>

namespace NEO {
enum class EngineGroupType : uint32_t;
struct HardwareInfo;
} // namespace NEO

namespace L0 {

struct Event;
struct Device;
struct EventPool;

class L0HwHelper {
  public:
    static L0HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupType groupType) const = 0;
    virtual L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const = 0;

    virtual bool isResumeWARequired() = 0;
    virtual bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool forceDefaultUsmCompressionSupport() const = 0;
    virtual bool isIpSamplingSupported(const NEO::HardwareInfo &hwInfo) const = 0;

    virtual void getAttentionBitmaskForSingleThreads(std::vector<ze_device_thread_t> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const = 0;
    virtual std::vector<ze_device_thread_t> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, const uint8_t *bitmask, const size_t bitmaskSize) const = 0;

  protected:
    L0HwHelper() = default;
};

template <typename GfxFamily>
class L0HwHelperHw : public L0HwHelper {
  public:
    static L0HwHelper &get() {
        static L0HwHelperHw<GfxFamily> l0HwHelper;
        return l0HwHelper;
    }
    void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupType groupType) const override;
    L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const override;
    L0HwHelperHw() = default;

    bool isResumeWARequired() override;
    bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool forceDefaultUsmCompressionSupport() const override;
    bool isIpSamplingSupported(const NEO::HardwareInfo &hwInfo) const override;
    void getAttentionBitmaskForSingleThreads(std::vector<ze_device_thread_t> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const override;
    std::vector<ze_device_thread_t> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, const uint8_t *bitmask, const size_t bitmaskSize) const override;
};

} // namespace L0
