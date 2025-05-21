/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/mt_helpers.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <mutex>
#include <optional>

namespace NEO {
class OSInterface;

struct DirectSubmissionProperties;
struct HardwareInfo;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext(uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);
    static OsContext *create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);

    bool isImmediateContextInitializationEnabled(bool isDefaultEngine) const;
    bool isInitialized() const { return contextInitialized; }
    bool ensureContextInitialized(bool allocateInterrupt);

    uint32_t getContextId() const { return contextId; }
    virtual uint64_t getOfflineDumpContextId(uint32_t deviceIndex) const { return 0; };
    uint32_t getNumSupportedDevices() const { return numSupportedDevices; }
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    const aub_stream::EngineType &getEngineType() const { return engineType; }
    EngineUsage getEngineUsage() const { return engineUsage; }
    void overrideEngineUsage(EngineUsage usage) { engineUsage = usage; }
    void overridePriority(int newPriority) {
        if (!priorityLevel.has_value()) {
            priorityLevel = newPriority;
        }
    }

    bool isRegular() const { return engineUsage == EngineUsage::regular; }
    bool isLowPriority() const { return engineUsage == EngineUsage::lowPriority; }
    bool isHighPriority() const { return engineUsage == EngineUsage::highPriority; }
    bool isInternalEngine() const { return engineUsage == EngineUsage::internal; }
    bool isCooperativeEngine() const { return engineUsage == EngineUsage::cooperative; }
    bool isRootDevice() const { return rootDevice; }
    virtual bool isDirectSubmissionSupported() const { return false; }
    bool isDefaultContext() const { return defaultContext; }
    void setDefaultContext(bool value) { defaultContext = value; }
    bool isDirectSubmissionActive() const { return directSubmissionActive; }
    bool isDebuggableContext() { return debuggableContext; }
    void setDirectSubmissionActive() { directSubmissionActive = true; }

    bool isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit);
    bool checkDirectSubmissionSupportsEngine(const DirectSubmissionProperties &directSubmissionProperty,
                                             aub_stream::EngineType contextEngineType,
                                             bool &startOnInit,
                                             bool &startInContext);
    virtual void reInitializeContext() {}

    inline static constexpr uint8_t getUmdPowerHintMax() { return NEO::OsContext::powerHintMax; }
    uint8_t getUmdPowerHintValue() { return powerHintValue; }
    void setUmdPowerHintValue(uint8_t powerHintValue) { this->powerHintValue = powerHintValue; }

    uint32_t getRootDeviceIndex() { return rootDeviceIndex; }

    void setNewResourceBound() {
        tlbFlushCounter++;
    };

    uint32_t peekTlbFlushCounter() const { return tlbFlushCounter.load(); }

    void setTlbFlushed(uint32_t newCounter) {
        NEO::MultiThreadHelpers::interlockedMax(lastFlushedTlbFlushCounter, newCounter);
    };
    bool isTlbFlushRequired() const {
        return (tlbFlushCounter.load() > lastFlushedTlbFlushCounter.load());
    };

    void setPrimaryContext(const OsContext *primary) {
        primaryContext = primary;
        isContextGroup = true;
    }
    const OsContext *getPrimaryContext() const {
        return primaryContext;
    }
    void setIsPrimaryEngine(const bool isPrimaryEngine) {
        this->isPrimaryEngine = isPrimaryEngine;
    }
    bool getIsPrimaryEngine() const {
        return this->isPrimaryEngine;
    }
    void setIsDefaultEngine(const bool isDefaultEngine) {
        this->isDefaultEngine = isDefaultEngine;
    }
    bool getIsDefaultEngine() const {
        return this->isDefaultEngine;
    }
    void setContextGroup(bool value) {
        isContextGroup = value;
    }
    bool isPartOfContextGroup() const {
        return isContextGroup;
    }
    virtual bool isDirectSubmissionLightActive() const { return false; }

  protected:
    virtual bool initializeContext(bool allocateInterrupt) { return true; }

    std::atomic<uint32_t> tlbFlushCounter{0};
    std::atomic<uint32_t> lastFlushedTlbFlushCounter{0};

    const uint32_t rootDeviceIndex;
    const uint32_t contextId;
    const DeviceBitfield deviceBitfield;
    const PreemptionMode preemptionMode;
    const uint32_t numSupportedDevices;
    aub_stream::EngineType engineType = aub_stream::ENGINE_RCS;
    EngineUsage engineUsage;
    std::optional<int> priorityLevel;
    const bool rootDevice = false;
    bool defaultContext = false;
    bool directSubmissionActive = false;
    std::once_flag contextInitializedFlag = {};
    bool contextInitialized = false;
    bool debuggableContext = false;
    uint8_t powerHintValue = 0;
    static constexpr inline uint8_t powerHintMax = 100u; // by definition: 100% power-saving

    bool isContextGroup = false;
    const OsContext *primaryContext = nullptr;
    bool isPrimaryEngine = false;
    bool isDefaultEngine = false;
};
} // namespace NEO
