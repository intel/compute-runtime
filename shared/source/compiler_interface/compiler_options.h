/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"
#include "shared/source/utilities/stackvec.h"

#include <algorithm>
#include <functional>

namespace NEO {
namespace CompilerOptions {
inline constexpr ConstStringRef greaterThan4gbBuffersRequired = "-cl-intel-greater-than-4GB-buffer-required";
inline constexpr ConstStringRef hasBufferOffsetArg = "-cl-intel-has-buffer-offset-arg";
inline constexpr ConstStringRef kernelDebugEnable = "-cl-kernel-debug-enable";
inline constexpr ConstStringRef arch32bit = "-m32";
inline constexpr ConstStringRef arch64bit = "-m64";
inline constexpr ConstStringRef debugKernelEnable = "-cl-kernel-debug-enable";
inline constexpr ConstStringRef optDisable = "-cl-opt-disable";
inline constexpr ConstStringRef argInfo = "-cl-kernel-arg-info";
inline constexpr ConstStringRef gtpinRera = "-cl-intel-gtpin-rera";
inline constexpr ConstStringRef finiteMathOnly = "-cl-finite-math-only";
inline constexpr ConstStringRef fastRelaxedMath = "-cl-fast-relaxed-math";
inline constexpr ConstStringRef preserveVec3Type = "-fpreserve-vec3-type";
inline constexpr ConstStringRef createLibrary = "-create-library";
inline constexpr ConstStringRef generateDebugInfo = "-g";
inline constexpr ConstStringRef generateSourcePath = "-s";
inline constexpr ConstStringRef bindlessMode = "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode";
inline constexpr ConstStringRef uniformWorkgroupSize = "-cl-uniform-work-group-size";
inline constexpr ConstStringRef forceEmuInt32DivRem = "-cl-intel-force-emu-int32divrem";
inline constexpr ConstStringRef forceEmuInt32DivRemSP = "-cl-intel-force-emu-sp-int32divrem";
inline constexpr ConstStringRef allowZebin = "-cl-intel-allow-zebin";
inline constexpr ConstStringRef disableZebin = "-cl-intel-disable-zebin";
inline constexpr ConstStringRef enableImageSupport = "-D__IMAGE_SUPPORT__=1";
inline constexpr ConstStringRef optLevel = "-ze-opt-level=O";
inline constexpr ConstStringRef excludeIrFromZebin = "-exclude-ir-from-zebin";
inline constexpr ConstStringRef noRecompiledFromIr = "-Wno-recompiled-from-ir";
inline constexpr ConstStringRef defaultGrf = "-cl-intel-128-GRF-per-thread";
inline constexpr ConstStringRef largeGrf = "-cl-intel-256-GRF-per-thread";
inline constexpr ConstStringRef autoGrf = "-cl-intel-enable-auto-large-GRF-mode";
inline constexpr ConstStringRef numThreadsPerEu = "-cl-intel-reqd-eu-thread-count";
inline constexpr ConstStringRef useCMCompiler = "-cmc";

inline constexpr size_t nullterminateSize = 1U;
inline constexpr size_t spaceSeparatorSize = 1U;

template <size_t Length>
constexpr size_t length(const char (&array)[Length]) {
    return Length;
}

constexpr size_t length(ConstStringRef string) {
    return string.length();
}

inline size_t length(const std::string &string) {
    return string.length();
}

constexpr size_t length(const char *string) {
    return constLength(string);
}

constexpr const char *data(ConstStringRef string) {
    return string.data();
}

inline const char *data(const std::string &string) {
    return string.data();
}

constexpr const char *data(const char *string) {
    return string;
}

template <typename T>
constexpr size_t concatenationLength(const T &t) {
    return length(t);
}

template <typename T, typename... RestT>
constexpr size_t concatenationLength(const T &arg, const RestT &...rest) {
    return length(arg) + spaceSeparatorSize + concatenationLength(rest...);
}

template <typename ContainerT, typename T>
inline void concatenateAppend(ContainerT &out, T &&arg) {
    if ((false == out.empty()) && (*out.rbegin() != ' ')) {
        out.push_back(' ');
    }
    out.insert(out.end(), data(arg), data(arg) + length(arg));
}

template <typename ContainerT, typename T, typename... RestT>
inline void concatenateAppend(ContainerT &out, T &&arg, RestT &&...rest) {
    concatenateAppend(out, std::forward<T>(arg));
    concatenateAppend(out, std::forward<RestT>(rest)...);
}

template <typename T, typename... RestT>
inline std::string concatenate(T &&arg, RestT &&...rest) {
    std::string ret;
    ret.reserve(nullterminateSize + concatenationLength(arg, rest...));
    concatenateAppend(ret, std::forward<T>(arg), std::forward<RestT>(rest)...);
    return ret;
}

template <size_t NumOptions>
constexpr size_t concatenationLength(const ConstStringRef (&options)[NumOptions]) {
    size_t ret = 0U;
    for (auto opt : options) {
        ret += spaceSeparatorSize + opt.length();
    }
    return (ret != 0U) ? ret - nullterminateSize : 0U;
}

template <typename ContainerT>
inline bool extract(const ConstStringRef &toBeExtracted, ContainerT &options) {
    const auto first{std::search(options.begin(), options.end(),
                                 std::default_searcher{toBeExtracted.begin(), toBeExtracted.end()})};

    if (first == options.end()) {
        return false;
    }

    const auto last{std::next(first, toBeExtracted.length())};
    options.erase(first, last);

    return true;
}

template <size_t MaxLength = 256>
class ConstConcatenation {
  public:
    template <size_t NumOptions>
    constexpr ConstConcatenation(const ConstStringRef (&options)[NumOptions]) {
        size_t i = 0U;
        for (auto opt : options) {
            for (size_t j = 0U, e = opt.length(); j < e; ++j, ++i) {
                storage[i] = opt[j];
            }
            storage[i] = ' ';
            ++i;
        }
        length = i;
        if (i > 0U) {
            storage[i - 1] = '\0';
        }
    }

    constexpr operator ConstStringRef() const {
        return ConstStringRef(storage, (length > 0U) ? (length - 1) : 0U);
    }

    constexpr operator const char *() const {
        return storage;
    }

  protected:
    char storage[MaxLength + nullterminateSize] = {};
    size_t length = 0U;
};

template <size_t MaxLength>
bool operator==(const ConstStringRef &lhs, const ConstConcatenation<MaxLength> &rhs) {
    return lhs == rhs.operator ConstStringRef();
}

template <size_t MaxLength>
bool operator==(const ConstConcatenation<MaxLength> &lhs, const ConstStringRef &rhs) {
    return rhs == lhs;
}

bool contains(const char *options, ConstStringRef optionToFind);

bool contains(const std::string &options, ConstStringRef optionToFind);

std::string wrapInQuotes(const std::string &stringToWrap);
using TokenizedString = StackVec<ConstStringRef, 32>;
TokenizedString tokenize(ConstStringRef src, char sperator = ' ');

void applyAdditionalInternalOptions(std::string &internalOptions);
void applyAdditionalApiOptions(std::string &apiOptions);
} // namespace CompilerOptions
} // namespace NEO
