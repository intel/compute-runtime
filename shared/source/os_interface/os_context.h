/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <memory>

namespace NEO {
class OSInterface;

struct DirectSubmissionProperties;
struct HardwareInfo;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext() = delete;

    static OsContext *create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice);
    uint32_t getContextId() const { return contextId; }
    uint32_t getNumSupportedDevices() const { return numSupportedDevices; }
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    aub_stream::EngineType &getEngineType() { return engineType; }
    bool isLowPriority() const { return engineUsage == EngineUsage::LowPriority; }
    bool isInternalEngine() const { return engineUsage == EngineUsage::Internal; }
    bool isRootDevice() const { return rootDevice; }
    virtual bool isDirectSubmissionSupported(const HardwareInfo &hwInfo) const { return false; }
    bool isDefaultContext() const { return defaultContext; }
    void setDefaultContext(bool value) { defaultContext = value; }
    bool isDirectSubmissionActive() { return directSubmissionActive; }
    void setDirectSubmissionActive() { directSubmissionActive = true; }

    bool isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit);
    bool checkDirectSubmissionSupportsEngine(const DirectSubmissionProperties &directSubmissionProperty,
                                             aub_stream::EngineType contextEngineType,
                                             bool &startOnInit,
                                             bool &startInContext);

  protected:
    OsContext(uint32_t contextId, DeviceBitfield deviceBitfield, EngineTypeUsage typeUsage, PreemptionMode preemptionMode, bool rootDevice)
        : contextId(contextId),
          deviceBitfield(deviceBitfield),
          preemptionMode(preemptionMode),
          numSupportedDevices(static_cast<uint32_t>(deviceBitfield.count())),
          engineType(typeUsage.first),
          engineUsage(typeUsage.second),
          rootDevice(rootDevice) {}

    const uint32_t contextId;
    const DeviceBitfield deviceBitfield;
    const PreemptionMode preemptionMode;
    const uint32_t numSupportedDevices;
    aub_stream::EngineType engineType = aub_stream::ENGINE_RCS;
    const EngineUsage engineUsage;
    const bool rootDevice = false;
    bool defaultContext = false;
    bool directSubmissionActive = false;
};
} // namespace NEO
