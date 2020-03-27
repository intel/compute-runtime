/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/directory.h"

#include <map>

using namespace NEO;

#undef DECLARE_DEBUG_VARIABLE

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

template <DebugFunctionalityLevel DebugLevel>
class TestDebugSettingsManager : public DebugSettingsManager<DebugLevel> {
  public:
    using DebugSettingsManager<DebugLevel>::dumpFlags;
    using DebugSettingsManager<DebugLevel>::settingsDumpFileName;

    TestDebugSettingsManager() : DebugSettingsManager<DebugLevel>("") {}
    SettingsReader *getSettingsReader() {
        return DebugSettingsManager<DebugLevel>::readerImpl.get();
    }
};

using FullyEnabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::Full>;
using FullyDisabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::None>;
