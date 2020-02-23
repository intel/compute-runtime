/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/linker.h"

#include <functional>
#include <tuple>

template <class BaseClass>
struct WhiteBox;

template <class BaseClass>
struct Mock;

template <>
struct WhiteBox<NEO::LinkerInput> : NEO::LinkerInput {
    using BaseClass = NEO::LinkerInput;

    using BaseClass::dataRelocations;
    using BaseClass::exportedFunctionsSegmentId;
    using BaseClass::relocations;
    using BaseClass::symbols;
    using BaseClass::traits;
    using BaseClass::valid;
};

template <typename MockT, typename ReturnT, typename... ArgsT>
struct LightMockConfig {
    using MockReturnT = ReturnT;
    using OverrideT = std::function<ReturnT(MockT *, ArgsT...)>;

    uint32_t timesCalled = 0U;
    OverrideT overrideFunc;
};

template <typename ConfigT, typename ObjT, typename... ArgsT>
typename ConfigT::MockReturnT invokeMocked(ConfigT &config, ObjT obj, ArgsT &&... args) {
    config.timesCalled += 1;
    if (config.overrideFunc) {
        return config.overrideFunc(obj, std::forward<ArgsT>(args)...);
    } else {
        return config.originalFunc(obj, std::forward<ArgsT>(args)...);
    }
}

#define LIGHT_MOCK_OVERRIDE_2(NAME, MOCK_T, BASE_T, RETURN_T, ARG0_T, ARG1_T)                                \
    struct : LightMockConfig<MOCK_T, RETURN_T, ARG0_T, ARG1_T> {                                             \
        OverrideT originalFunc = +[](BaseT *obj, ARG0_T arg0, ARG1_T arg1) -> RETURN_T {                     \
            return obj->BaseT::NAME(std::forward<ARG0_T>(arg0), std::forward<ARG1_T>(arg1));                 \
        };                                                                                                   \
    } NAME##MockConfig;                                                                                      \
                                                                                                             \
    RETURN_T NAME(ARG0_T arg0, ARG1_T arg1) override {                                                       \
        return invokeMocked(NAME##MockConfig, this, std::forward<ARG0_T>(arg0), std::forward<ARG1_T>(arg1)); \
    }

#define LIGHT_MOCK_OVERRIDE_3(NAME, MOCK_T, BASE_T, RETURN_T, ARG0_T, ARG1_T, ARG2_T)                                                    \
    struct : LightMockConfig<MOCK_T, RETURN_T, ARG0_T, ARG1_T, ARG2_T> {                                                                 \
        OverrideT originalFunc = +[](BaseT *obj, ARG0_T arg0, ARG1_T arg1, ARG2_T arg2) -> RETURN_T {                                    \
            return obj->BaseT::NAME(std::forward<ARG0_T>(arg0), std::forward<ARG1_T>(arg1), std::forward<ARG2_T>(arg2));                 \
        };                                                                                                                               \
    } NAME##MockConfig;                                                                                                                  \
                                                                                                                                         \
    RETURN_T NAME(ARG0_T arg0, ARG1_T arg1, ARG2_T arg2) override {                                                                      \
        return invokeMocked(NAME##MockConfig, this, std::forward<ARG0_T>(arg0), std::forward<ARG1_T>(arg1), std::forward<ARG2_T>(arg2)); \
    }

template <>
struct Mock<NEO::LinkerInput> : WhiteBox<NEO::LinkerInput> {
    using ThisT = Mock<NEO::LinkerInput>;
    using BaseT = NEO::LinkerInput;
    using WhiteBoxBaseT = WhiteBox<BaseT>;

    LIGHT_MOCK_OVERRIDE_2(decodeGlobalVariablesSymbolTable, ThisT, BaseT, bool, const void *, uint32_t);
    LIGHT_MOCK_OVERRIDE_3(decodeExportedFunctionsSymbolTable, ThisT, BaseT, bool, const void *, uint32_t, uint32_t);
    LIGHT_MOCK_OVERRIDE_3(decodeRelocationTable, ThisT, BaseT, bool, const void *, uint32_t, uint32_t);
};
