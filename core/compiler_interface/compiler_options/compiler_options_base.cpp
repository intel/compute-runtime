/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "compiler_options_base.h"

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

} // namespace CompilerOptions
} // namespace NEO
