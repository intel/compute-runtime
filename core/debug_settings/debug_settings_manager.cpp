/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings_manager.h"

#include "core/debug_settings/definitions/translate_debug_settings.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/string.h"
#include "core/utilities/debug_settings_reader_creator.h"

#include <cstdio>
#include <sstream>

namespace std {
static std::string to_string(const std::string &arg) {
    return arg;
}
} // namespace std

namespace NEO {

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager(const char *registryPath) {
    if (registryReadAvailable()) {
        readerImpl = SettingsReaderCreator::create(std::string(registryPath));
        injectSettingsFromReader();
        dumpFlags();
    }
    translateDebugSettings(flags);
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() = default;

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::getHardwareInfoOverride(std::string &hwInfoConfig) {
    std::string str = flags.HardwareInfoOverride.get();
    if (str[0] == '\"') {
        str.pop_back();
        hwInfoConfig = str.substr(1, std::string::npos);
    } else {
        hwInfoConfig = str;
    }
}

template <DebugFunctionalityLevel DebugLevel>
template <typename DataType>
void DebugSettingsManager<DebugLevel>::dumpNonDefaultFlag(const char *variableName, const DataType &variableValue, const DataType &defaultValue) {
    if (variableValue != defaultValue) {
        const auto variableStringValue = std::to_string(variableValue);
        printDebugString(true, stdout, "Non-default value of debug variable: %s = %s\n", variableName, variableStringValue.c_str());
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpFlags() const {
    if (flags.PrintDebugSettings.get() == false) {
        return;
    }

    std::ofstream settingsDumpFile{settingsDumpFileName, std::ios::out};
    DEBUG_BREAK_IF(!settingsDumpFile.good());

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)   \
    settingsDumpFile << #variableName << " = " << flags.variableName.get() << '\n'; \
    dumpNonDefaultFlag(#variableName, flags.variableName.get(), defaultValue);
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::injectSettingsFromReader() {
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)            \
    {                                                                                        \
        dataType tempData = readerImpl->getSetting(#variableName, flags.variableName.get()); \
        flags.variableName.set(tempData);                                                    \
    }
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template class DebugSettingsManager<DebugFunctionalityLevel::None>;
template class DebugSettingsManager<DebugFunctionalityLevel::Full>;
template class DebugSettingsManager<DebugFunctionalityLevel::RegKeys>;
}; // namespace NEO
