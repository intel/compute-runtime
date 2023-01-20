/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/api_gfx_core_helper.h"

#include "level_zero/tools/source/debug/eu_thread.h"
#include <level_zero/ze_api.h>

#include "igfxfmid.h"

#include <memory>
#include <vector>

namespace NEO {
enum class EngineGroupType : uint32_t;
struct HardwareInfo;
struct EngineGroupT;
struct RootDeviceEnvironment;
class Debugger;
} // namespace NEO

namespace L0 {

struct Event;
struct Device;
struct EventPool;

class L0GfxCoreHelper;
using createL0GfxCoreHelperFunctionType = std::unique_ptr<L0GfxCoreHelper> (*)();

class L0GfxCoreHelper : public NEO::ApiGfxCoreHelper {
  public:
    ~L0GfxCoreHelper() override = default;
    static std::unique_ptr<L0GfxCoreHelper> create(GFXCORE_FAMILY gfxCore);

    static bool enableFrontEndStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enablePipelineSelectStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enableStateComputeModeTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enableImmediateCmdListHeapSharing(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool cmdlistSupport);
    static bool usePipeControlMultiKernelEventSync(const NEO::HardwareInfo &hwInfo);
    static bool useCompactL3FlushEventPacket(const NEO::HardwareInfo &hwInfo);
    static bool useDynamicEventPacketsCount(const NEO::HardwareInfo &hwInfo);
    static bool useSignalAllEventPackets(const NEO::HardwareInfo &hwInfo);
    virtual void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const = 0;
    virtual L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const = 0;

    virtual bool isResumeWARequired() = 0;
    virtual bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool forceDefaultUsmCompressionSupport() const = 0;

    virtual void getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const = 0;
    virtual std::vector<EuThread::ThreadId> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const = 0;
    virtual bool multiTileCapablePlatform() const = 0;
    virtual bool alwaysAllocateEventInLocalMem() const = 0;
    virtual bool platformSupportsCmdListHeapSharing() const = 0;
    virtual bool platformSupportsStateComputeModeTracking() const = 0;
    virtual bool platformSupportsFrontEndTracking() const = 0;
    virtual bool platformSupportsPipelineSelectTracking() const = 0;
    virtual bool platformSupportsRayTracing() const = 0;
    virtual bool isZebinAllowed(const NEO::Debugger *debugger) const = 0;
    virtual uint32_t getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const = 0;

  protected:
    L0GfxCoreHelper() = default;
};

template <typename GfxFamily>
class L0GfxCoreHelperHw : public L0GfxCoreHelper {
  public:
    ~L0GfxCoreHelperHw() override = default;
    static std::unique_ptr<L0GfxCoreHelper> create() {
        return std::unique_ptr<L0GfxCoreHelper>(new L0GfxCoreHelperHw<GfxFamily>());
    }

    void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const override;
    L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const override;

    bool isResumeWARequired() override;
    bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool forceDefaultUsmCompressionSupport() const override;
    void getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const override;
    std::vector<EuThread::ThreadId> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const override;
    bool multiTileCapablePlatform() const override;
    bool alwaysAllocateEventInLocalMem() const override;
    bool platformSupportsCmdListHeapSharing() const override;
    bool platformSupportsStateComputeModeTracking() const override;
    bool platformSupportsFrontEndTracking() const override;
    bool platformSupportsPipelineSelectTracking() const override;
    bool platformSupportsRayTracing() const override;
    bool isZebinAllowed(const NEO::Debugger *debugger) const override;
    uint32_t getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const override;
    uint32_t getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const override;

  protected:
    L0GfxCoreHelperHw() = default;
};

} // namespace L0
