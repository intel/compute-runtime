/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/file_io.h"
#include "core/utilities/directory.h"
#include "runtime/helpers/string_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <map>

using namespace NEO;
using namespace std;

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

    static bool isEqual(string returnedValue, string defaultValue) {
        return returnedValue == defaultValue;
    }
};

template <DebugFunctionalityLevel DebugLevel>
class TestDebugSettingsManager : public DebugSettingsManager<DebugLevel> {
  public:
    using DebugSettingsManager<DebugLevel>::dumpFlags;
    using DebugSettingsManager<DebugLevel>::settingsDumpFileName;

    SettingsReader *getSettingsReader() {
        return DebugSettingsManager<DebugLevel>::readerImpl.get();
    }
};

using FullyEnabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::Full>;
using FullyDisabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::None>;
