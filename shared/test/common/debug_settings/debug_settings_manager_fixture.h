/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/stackvec.h"

#include <map>

using namespace NEO;

#undef DECLARE_DEBUG_VARIABLE

namespace NEO {
extern std::unique_ptr<SettingsReader> mockSettingsReader;
extern const StackVec<DebugVarPrefix, 4> *validUltPrefixTypesOverride;
} // namespace NEO

class TestDebugFlagsChecker {
  public:
    static bool isEqual(int32_t returnedValue, bool defaultValue) {
        if (returnedValue == 0) {
            return !defaultValue;
        } else {
            return defaultValue;
        }
    }

    static bool isEqual(int32_t returnedValue, int32_t defaultValue) {
        return returnedValue == defaultValue;
    }

    static bool isEqual(int64_t returnedValue, int64_t defaultValue) {
        return returnedValue == defaultValue;
    }

    static bool isEqual(std::string returnedValue, std::string defaultValue) {
        return returnedValue == defaultValue;
    }
};

template <DebugFunctionalityLevel debugLevel>
class TestDebugSettingsManager : public DebugSettingsManager<debugLevel> {
  public:
    using DebugSettingsManager<debugLevel>::dumpFlags;
    using DebugSettingsManager<debugLevel>::settingsDumpFileName;

    TestDebugSettingsManager() : DebugSettingsManager<debugLevel>("") {}
    SettingsReader *getSettingsReader() {
        return DebugSettingsManager<debugLevel>::readerImpl.get();
    }
};

using FullyEnabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::full>;
using FullyDisabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::none>;
