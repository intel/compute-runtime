/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstring>

namespace NEO {
namespace CompilerOptions {

bool contains(const char *options, ConstStringRef optionToFind) {
    auto it = strstr(options, optionToFind.data());
    while (it != nullptr) {
        const auto delimiter = it[optionToFind.size()];
        if ((' ' == delimiter) || ('\0' == delimiter)) {
            if ((it == options) || (it[-1] == ' ')) {
                return true;
            }
        }
        it = strstr(it + 1, optionToFind.data());
    }
    return false;
}

bool contains(const std::string &options, ConstStringRef optionToFind) {
    return contains(options.c_str(), optionToFind);
}

std::string wrapInQuotes(const std::string &stringToWrap) {
    std::string quoteEscape{"\""};
    return std::string{quoteEscape + stringToWrap + quoteEscape};
}

TokenizedString tokenize(ConstStringRef src, char sperator) {
    TokenizedString ret;
    const char *it = src.begin();
    while (it < src.end()) {
        const char *beg = it;
        while ((beg < src.end()) && (*beg == sperator)) {
            ++beg;
        }
        const char *end = beg;
        while ((end < src.end()) && (*end != sperator)) {
            ++end;
        }
        it = end;
        if (end != beg) {
            ret.push_back(ConstStringRef(beg, end - beg));
        }
    }
    return ret;
};

void applyAdditionalInternalOptions(std::string &internalOptions) {
    size_t pos;
    if (DebugManager.flags.ForceLargeGrfCompilationMode.get()) {
        pos = internalOptions.find(CompilerOptions::largeGrf.data());
        if (pos == std::string::npos) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::largeGrf);
        }
    } else if (DebugManager.flags.ForceDefaultGrfCompilationMode.get()) {
        pos = internalOptions.find(CompilerOptions::defaultGrf.data());
        if (pos == std::string::npos) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::defaultGrf.data());
        }
        pos = internalOptions.find(CompilerOptions::largeGrf.data());
        if (pos != std::string::npos) {
            internalOptions.erase(pos, CompilerOptions::largeGrf.size());
        }
    }
}

void applyAdditionalApiOptions(std::string &apiOptions) {
    size_t pos;
    if (DebugManager.flags.ForceAutoGrfCompilationMode.get() == 1) {
        pos = apiOptions.find(CompilerOptions::autoGrf.data());
        if (pos == std::string::npos) {
            CompilerOptions::concatenateAppend(apiOptions, CompilerOptions::autoGrf);
        }
        pos = apiOptions.find(CompilerOptions::largeGrf.data());
        if (pos != std::string::npos) {
            apiOptions.erase(pos, CompilerOptions::largeGrf.size());
        }
    }
}

} // namespace CompilerOptions
} // namespace NEO
