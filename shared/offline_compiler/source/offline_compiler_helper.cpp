/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_reg_path.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {

void translateDebugSettings(DebugVariables &debugVariables) {
}

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    static const StackVec<const char *, 4> validOclocPrefixes = {"NEO_OCLOC_"};
    return validOclocPrefixes;
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    static const StackVec<DebugVarPrefix, 4> validOclocPrefixTypes = {DebugVarPrefix::neoOcloc};
    return validOclocPrefixTypes;
}

DebugSettingsManager<globalDebugFunctionalityLevel> debugManager(Ocloc::oclocRegPath);
} // namespace NEO
