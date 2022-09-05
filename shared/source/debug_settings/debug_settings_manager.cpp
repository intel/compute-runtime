/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings_manager.h"

#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace NEO {

template <typename T>
static std::string toString(const T &arg) {
    if constexpr (std::is_convertible_v<std::string, T>) {
        return static_cast<std::string>(arg);
    } else {
        return std::to_string(arg);
    }
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::DebugSettingsManager(const char *registryPath) {
    readerImpl = SettingsReaderCreator::create(std::string(registryPath));
    injectSettingsFromReader();
    dumpFlags();
    translateDebugSettings(flags);

    while (isLoopAtDriverInitEnabled())
        ;
}

template <DebugFunctionalityLevel DebugLevel>
DebugSettingsManager<DebugLevel>::~DebugSettingsManager() {
    readerImpl.reset();
};

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
        const auto variableStringValue = toString(variableValue);
        PRINT_DEBUG_STRING(true, stdout, "Non-default value of debug variable: %s = %s\n", variableName, variableStringValue.c_str());
    }
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::dumpFlags() const {
    if (flags.PrintDebugSettings.get() == false) {
        return;
    }

    std::ofstream settingsDumpFile{settingsDumpFileName, std::ios::out};
    DEBUG_BREAK_IF(!settingsDumpFile.good());

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                         \
    settingsDumpFile << getNonReleaseKeyName(#variableName) << " = " << flags.variableName.get() << '\n'; \
    dumpNonDefaultFlag<dataType>(getNonReleaseKeyName(#variableName), flags.variableName.get(), defaultValue);

    if (registryReadAvailable() || isDebugKeysReadEnabled()) {
#include "debug_variables.inl"
    }
#undef DECLARE_DEBUG_VARIABLE

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)   \
    settingsDumpFile << #variableName << " = " << flags.variableName.get() << '\n'; \
    dumpNonDefaultFlag<dataType>(#variableName, flags.variableName.get(), defaultValue);
#include "release_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template <DebugFunctionalityLevel DebugLevel>
void DebugSettingsManager<DebugLevel>::injectSettingsFromReader() {
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                                  \
    {                                                                                                              \
        dataType tempData = readerImpl->getSetting(getNonReleaseKeyName(#variableName), flags.variableName.get()); \
        flags.variableName.set(tempData);                                                                          \
    }

    if (registryReadAvailable() || isDebugKeysReadEnabled()) {
#include "debug_variables.inl"
    }

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)            \
    {                                                                                        \
        dataType tempData = readerImpl->getSetting(#variableName, flags.variableName.get()); \
        flags.variableName.set(tempData);                                                    \
    }
#include "release_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

template class DebugSettingsManager<DebugFunctionalityLevel::None>;
template class DebugSettingsManager<DebugFunctionalityLevel::Full>;
template class DebugSettingsManager<DebugFunctionalityLevel::RegKeys>;
}; // namespace NEO
