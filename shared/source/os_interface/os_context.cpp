/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/direct_submission/direct_submission_properties.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
bool OsContext::isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit) {
    if (DebugManager.flags.EnableDirectSubmission.get() == 1) {
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
    if (contextEngineType == aub_stream::ENGINE_BCS) {
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