/*
 * Copyright (C) 2019-2025 Intel Corporation
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

StackVec<const char *, 4> validOclocPrefixes;
StackVec<NEO::DebugVarPrefix, 4> validOclocPrefixTypes;

void translateDebugSettings(DebugVariables &debugVariables) {
}

void ApiSpecificConfig::initPrefixes() {
    validOclocPrefixes = {"NEO_OCLOC_"};
    validOclocPrefixTypes = {DebugVarPrefix::neoOcloc};
}

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    return validOclocPrefixes;
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    return validOclocPrefixTypes;
}

DebugSettingsManager<globalDebugFunctionalityLevel> debugManager(Ocloc::oclocRegPath);
} // namespace NEO
