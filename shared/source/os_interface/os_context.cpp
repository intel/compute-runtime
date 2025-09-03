/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_properties.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
OsContext::OsContext(uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
    : rootDeviceIndex(rootDeviceIndex),
      contextId(contextId),
      deviceBitfield(engineDescriptor.deviceBitfield),
      preemptionMode(engineDescriptor.preemptionMode),
      numSupportedDevices(static_cast<uint32_t>(engineDescriptor.deviceBitfield.count())),
      engineType(engineDescriptor.engineTypeUsage.first),
      engineUsage(engineDescriptor.engineTypeUsage.second),
      rootDevice(engineDescriptor.isRootDevice) {}

bool OsContext::isImmediateContextInitializationEnabled(bool isDefaultEngine) const {
    if (debugManager.flags.DeferOsContextInitialization.get() == 0) {
        return true;
    }

    if (engineUsage == EngineUsage::internal) {
        return true;
    }

    if (isDefaultEngine) {
        return true;
    }

    if (engineType == aub_stream::EngineType::ENGINE_BCS && ApiSpecificConfig::ApiType::OCL == ApiSpecificConfig::getApiType()) {
        return true;
    }

    return false;
}

bool OsContext::ensureContextInitialized(bool allocateInterrupt) {
    std::call_once(contextInitializedFlag, [this, allocateInterrupt] {
        if (debugManager.flags.PrintOsContextInitializations.get()) {
            printf("OsContext initialization: contextId=%d usage=%s type=%s isRootDevice=%d\n",
                   contextId,
                   EngineHelpers::engineUsageToString(engineUsage).c_str(),
                   EngineHelpers::engineTypeToString(engineType).c_str(),
                   static_cast<int>(rootDevice));
        }

        if (!initializeContext(allocateInterrupt)) {
            contextInitialized = false;
        } else {
            contextInitialized = true;
        }
    });
    return contextInitialized;
}

bool OsContext::isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit) {
    bool enableDirectSubmission = this->isDirectSubmissionSupported() &&
                                  this->getUmdPowerHintValue() < NEO::OsContext::getUmdPowerHintMax();

    if (debugManager.flags.SetCommandStreamReceiver.get() > 0) {
        enableDirectSubmission = false;
    }

    if (debugManager.flags.EnableDirectSubmission.get() != -1) {
        enableDirectSubmission = debugManager.flags.EnableDirectSubmission.get();
    }

    if (enableDirectSubmission) {
        auto contextEngineType = this->getEngineType();
        const DirectSubmissionProperties &directSubmissionProperty =
            hwInfo.capabilityTable.directSubmissionEngines.data[contextEngineType];

        bool startDirect = true;
        if (!this->isDefaultContext()) {
            startDirect = directSubmissionProperty.useNonDefault;
        }
        if (this->isLowPriority()) {
            startDirect = false;
        }
        if (this->isInternalEngine()) {
            startDirect = false;
        }
        if (this->isRootDevice()) {
            startDirect = directSubmissionProperty.useRootDevice;
        }

        submitOnInit = directSubmissionProperty.submitOnInit;
        bool engineSupported = checkDirectSubmissionSupportsEngine(directSubmissionProperty,
                                                                   contextEngineType,
                                                                   submitOnInit,
                                                                   startDirect);

        if (engineSupported && startDirect) {
            this->setDirectSubmissionActive();
        }

        return engineSupported && startDirect;
    }
    return false;
}

bool OsContext::checkDirectSubmissionSupportsEngine(const DirectSubmissionProperties &directSubmissionProperty, aub_stream::EngineType contextEngineType, bool &startOnInit, bool &startInContext) {
    bool supported = directSubmissionProperty.engineSupported;
    startOnInit = directSubmissionProperty.submitOnInit;
    if (EngineHelpers::isBcs(contextEngineType)) {
        int32_t blitterOverrideKey = debugManager.flags.DirectSubmissionOverrideBlitterSupport.get();
        if (blitterOverrideKey != -1) {
            supported = blitterOverrideKey == 0 ? false : true;
            startOnInit = blitterOverrideKey == 1 ? true : false;
        }
    } else if (EngineHelpers::isCcs(contextEngineType)) {
        int32_t computeOverrideKey = debugManager.flags.DirectSubmissionOverrideComputeSupport.get();
        if (computeOverrideKey != -1) {
            supported = computeOverrideKey == 0 ? false : true;
            startOnInit = computeOverrideKey == 1 ? true : false;
        }
    } else {
        // assume else is RCS
        int32_t renderOverrideKey = debugManager.flags.DirectSubmissionOverrideRenderSupport.get();
        if (renderOverrideKey != -1) {
            supported = renderOverrideKey == 0 ? false : true;
            startOnInit = renderOverrideKey == 1 ? true : false;
        }
    }

    // enable start in context only when default support is overridden and enabled
    if (supported && !directSubmissionProperty.engineSupported) {
        startInContext = true;
    }

    return supported;
}

} // namespace NEO
