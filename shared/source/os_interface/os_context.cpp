/*
 * Copyright (C) 2021 Intel Corporation
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
OsContext::OsContext(uint32_t contextId, const EngineDescriptor &engineDescriptor)
    : contextId(contextId),
      deviceBitfield(engineDescriptor.deviceBitfield),
      preemptionMode(engineDescriptor.preemptionMode),
      numSupportedDevices(static_cast<uint32_t>(engineDescriptor.deviceBitfield.count())),
      engineType(engineDescriptor.engineTypeUsage.first),
      engineUsage(engineDescriptor.engineTypeUsage.second),
      rootDevice(engineDescriptor.isRootDevice),
      engineInstancedDevice(engineDescriptor.isEngineInstanced) {}

bool OsContext::isImmediateContextInitializationEnabled(bool isDefaultEngine) const {
    if (DebugManager.flags.DeferOsContextInitialization.get() == 0) {
        return true;
    }

    if (engineUsage == EngineUsage::Internal) {
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

void OsContext::ensureContextInitialized() {
    std::call_once(contextInitializedFlag, [this] {
        if (DebugManager.flags.PrintOsContextInitializations.get()) {
            printf("OsContext initialization: contextId=%d usage=%s type=%s isRootDevice=%d\n",
                   contextId,
                   EngineHelpers::engineUsageToString(engineUsage).c_str(),
                   EngineHelpers::engineTypeToString(engineType).c_str(),
                   static_cast<int>(rootDevice));
        }

        initializeContext();
        contextInitialized = true;
    });
}

bool OsContext::isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit) {
    bool enableDirectSubmission = this->isDirectSubmissionSupported(hwInfo);

    if (DebugManager.flags.EnableDirectSubmission.get() != -1) {
        enableDirectSubmission = DebugManager.flags.EnableDirectSubmission.get();
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
            startDirect = directSubmissionProperty.useLowPriority;
        }
        if (this->isInternalEngine()) {
            startDirect = directSubmissionProperty.useInternal;
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
        int32_t blitterOverrideKey = DebugManager.flags.DirectSubmissionOverrideBlitterSupport.get();
        if (blitterOverrideKey != -1) {
            supported = blitterOverrideKey == 0 ? false : true;
            startOnInit = blitterOverrideKey == 1 ? true : false;
        }
    } else if (contextEngineType == aub_stream::ENGINE_RCS) {
        int32_t renderOverrideKey = DebugManager.flags.DirectSubmissionOverrideRenderSupport.get();
        if (renderOverrideKey != -1) {
            supported = renderOverrideKey == 0 ? false : true;
            startOnInit = renderOverrideKey == 1 ? true : false;
        }
    } else {
        //assume else is CCS
        int32_t computeOverrideKey = DebugManager.flags.DirectSubmissionOverrideComputeSupport.get();
        if (computeOverrideKey != -1) {
            supported = computeOverrideKey == 0 ? false : true;
            startOnInit = computeOverrideKey == 1 ? true : false;
        }
    }

    //enable start in context only when default support is overridden and enabled
    if (supported && !directSubmissionProperty.engineSupported) {
        startInContext = true;
    }

    return supported;
}

} // namespace NEO
