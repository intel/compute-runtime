/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/api_gfx_core_helper.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/heap_base_address_model.h"

#include "level_zero/driver_experimental/zex_graph.h"
#include "level_zero/tools/source/debug/eu_thread.h"
#include "level_zero/zet_intel_gpu_debug.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "neo_igfxfmid.h"

#include <map>
#include <memory>
#include <vector>

namespace NEO {
enum class EngineGroupType : uint32_t;
struct HardwareInfo;
struct EngineGroupT;
struct RootDeviceEnvironment;
class Debugger;
class ProductHelper;
class TagAllocatorBase;
class MemoryManager;
} // namespace NEO

class RootDeviceIndicesContainer;

namespace L0 {

enum class RTASDeviceFormatInternal {
    version1 = 1,
    version2 = 2,
};

struct CopyOffloadMode;
struct Event;
struct Device;
struct EventPool;
struct EventDescriptor;

class L0GfxCoreHelper;
using createL0GfxCoreHelperFunctionType = std::unique_ptr<L0GfxCoreHelper> (*)();

class L0GfxCoreHelper : public NEO::ApiGfxCoreHelper {
  public:
    ~L0GfxCoreHelper() override = default;
    static std::unique_ptr<L0GfxCoreHelper> create(GFXCORE_FAMILY gfxCore);

    static bool enableFrontEndStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enablePipelineSelectStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enableStateComputeModeTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enableStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool enableImmediateCmdListHeapSharing(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool cmdlistSupport);
    static bool usePipeControlMultiKernelEventSync(const NEO::HardwareInfo &hwInfo);
    static bool useCompactL3FlushEventPacket(const NEO::HardwareInfo &hwInfo, bool flushL3AfterPostSync);
    static bool useDynamicEventPacketsCount(const NEO::HardwareInfo &hwInfo);
    static bool useSignalAllEventPackets(const NEO::HardwareInfo &hwInfo);
    static NEO::HeapAddressModel getHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static bool dispatchCmdListBatchBufferAsPrimary(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool allowPrimary);
    static bool useImmediateComputeFlushTask(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static ze_mutable_command_exp_flags_t getCmdListUpdateCapabilities(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);
    static ze_record_replay_graph_exp_flags_t getRecordReplayGraphCapabilities(const NEO::RootDeviceEnvironment &rootDeviceEnvironment);

    virtual void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const = 0;
    virtual L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device, ze_result_t &result) const = 0;
    virtual L0::Event *createStandaloneEvent(const EventDescriptor &desc, L0::Device *device, ze_result_t &result) const = 0;

    virtual bool isResumeWARequired() = 0;
    virtual bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual bool forceDefaultUsmCompressionSupport() const = 0;

    virtual void getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const = 0;
    virtual std::vector<EuThread::ThreadId> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const = 0;
    virtual bool threadResumeRequiresUnlock() const = 0;
    virtual bool isThreadControlStoppedSupported() const = 0;
    virtual bool alwaysAllocateEventInLocalMem() const = 0;
    virtual bool platformSupportsCmdListHeapSharing() const = 0;
    virtual bool platformSupportsStateComputeModeTracking() const = 0;
    virtual bool platformSupportsFrontEndTracking() const = 0;
    virtual bool platformSupportsPipelineSelectTracking() const = 0;
    virtual bool platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual bool platformSupportsPrimaryBatchBufferCmdList() const = 0;
    virtual uint32_t getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getEventBaseMaxPacketCount(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual NEO::HeapAddressModel getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual ze_rtas_format_exp_t getSupportedRTASFormatExp() const = 0;
    virtual ze_rtas_format_ext_t getSupportedRTASFormatExt() const = 0;
    virtual bool platformSupportsImmediateComputeFlushTask() const = 0;
    virtual zet_debug_regset_type_intel_gpu_t getRegsetTypeForLargeGrfDetection() const = 0;
    virtual uint32_t getGrfRegisterCount(uint32_t *regPtr) const = 0;
    virtual uint32_t getCmdListWaitOnMemoryDataSize() const = 0;
    virtual bool hasUnifiedPostSyncAllocationLayout() const = 0;
    virtual uint32_t getImmediateWritePostSyncOffset() const = 0;
    virtual ze_mutable_command_exp_flags_t getPlatformCmdListUpdateCapabilities() const = 0;
    virtual ze_record_replay_graph_exp_flags_t getPlatformRecordReplayGraphCapabilities() const = 0;
    virtual void appendPlatformSpecificExtensions(std::vector<std::pair<std::string, uint32_t>> &extensions, const NEO::ProductHelper &productHelper, const NEO::HardwareInfo &hwInfo) const = 0;
    virtual std::vector<std::pair<const char *, const char *>> getStallSamplingReportMetrics() const = 0;
    virtual void stallSumIpDataToTypedValues(uint64_t ip, void *sumIpData, std::vector<zet_typed_value_t> &ipDataValues) = 0;
    virtual bool stallIpDataMapUpdate(std::map<uint64_t, void *> &stallSumIpDataMap, const uint8_t *pRawIpData) = 0;
    virtual void stallIpDataMapDelete(std::map<uint64_t, void *> &stallSumIpDataMap) = 0;
    virtual uint32_t getIpSamplingMetricCount() = 0;
    virtual uint64_t getIpSamplingIpMask() const = 0;
    virtual bool synchronizedDispatchSupported() const = 0;
    virtual bool implicitSynchronizedDispatchForCooperativeKernelsAllowed() const = 0;
    virtual std::unique_ptr<NEO::TagAllocatorBase> getInOrderTimestampAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, NEO::MemoryManager *memoryManager, size_t initialTagCount, size_t packetsCountPerElement, size_t tagAlignment,
                                                                                NEO::DeviceBitfield deviceBitfield) const = 0;
    virtual uint64_t getOaTimestampValidBits() const = 0;
    virtual CopyOffloadMode getDefaultCopyOffloadMode(bool additionalBlitPropertiesSupported) const = 0;
    virtual bool isDefaultCmdListWithCopyOffloadSupported(bool additionalBlitPropertiesSupported) const = 0;
    virtual bool bcsSplitAggregatedModeEnabled() const = 0;
    virtual bool supportMetricsAggregation() const = 0;

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
    L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device, ze_result_t &result) const override;
    L0::Event *createStandaloneEvent(const EventDescriptor &desc, L0::Device *device, ze_result_t &result) const override;

    bool isResumeWARequired() override;
    bool imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const override;
    bool forceDefaultUsmCompressionSupport() const override;
    void getAttentionBitmaskForSingleThreads(const std::vector<EuThread::ThreadId> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) const override;
    std::vector<EuThread::ThreadId> getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, uint32_t tile, const uint8_t *bitmask, const size_t bitmaskSize) const override;
    bool threadResumeRequiresUnlock() const override;
    bool isThreadControlStoppedSupported() const override;
    bool alwaysAllocateEventInLocalMem() const override;
    bool platformSupportsCmdListHeapSharing() const override;
    bool platformSupportsStateComputeModeTracking() const override;
    bool platformSupportsFrontEndTracking() const override;
    bool platformSupportsPipelineSelectTracking() const override;
    bool platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const override;
    bool platformSupportsPrimaryBatchBufferCmdList() const override;
    uint32_t getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const override;
    uint32_t getEventBaseMaxPacketCount(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const override;
    NEO::HeapAddressModel getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const override;
    ze_rtas_format_exp_t getSupportedRTASFormatExp() const override;
    ze_rtas_format_ext_t getSupportedRTASFormatExt() const override;
    bool platformSupportsImmediateComputeFlushTask() const override;
    zet_debug_regset_type_intel_gpu_t getRegsetTypeForLargeGrfDetection() const override;
    uint32_t getGrfRegisterCount(uint32_t *regPtr) const override;
    uint32_t getCmdListWaitOnMemoryDataSize() const override;
    bool hasUnifiedPostSyncAllocationLayout() const override;
    uint32_t getImmediateWritePostSyncOffset() const override;
    ze_mutable_command_exp_flags_t getPlatformCmdListUpdateCapabilities() const override;
    ze_record_replay_graph_exp_flags_t getPlatformRecordReplayGraphCapabilities() const override;
    void appendPlatformSpecificExtensions(std::vector<std::pair<std::string, uint32_t>> &extensions, const NEO::ProductHelper &productHelper, const NEO::HardwareInfo &hwInfo) const override;
    std::vector<std::pair<const char *, const char *>> getStallSamplingReportMetrics() const override;
    void stallSumIpDataToTypedValues(uint64_t ip, void *sumIpData, std::vector<zet_typed_value_t> &ipDataValues) override;
    bool stallIpDataMapUpdate(std::map<uint64_t, void *> &stallSumIpDataMap, const uint8_t *pRawIpData) override;
    void stallIpDataMapDelete(std::map<uint64_t, void *> &stallSumIpDataMap) override;
    uint32_t getIpSamplingMetricCount() override;
    uint64_t getIpSamplingIpMask() const override;
    bool synchronizedDispatchSupported() const override;
    bool implicitSynchronizedDispatchForCooperativeKernelsAllowed() const override;
    std::unique_ptr<NEO::TagAllocatorBase> getInOrderTimestampAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, NEO::MemoryManager *memoryManager, size_t initialTagCount, size_t packetsCountPerElement, size_t tagAlignment,
                                                                        NEO::DeviceBitfield deviceBitfield) const override;
    uint64_t getOaTimestampValidBits() const override;
    CopyOffloadMode getDefaultCopyOffloadMode(bool additionalBlitPropertiesSupported) const override;
    bool isDefaultCmdListWithCopyOffloadSupported(bool additionalBlitPropertiesSupported) const override;
    bool bcsSplitAggregatedModeEnabled() const override;
    bool supportMetricsAggregation() const override;

  protected:
    L0GfxCoreHelperHw() = default;
};

} // namespace L0
