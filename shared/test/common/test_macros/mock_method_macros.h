/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"

#include <type_traits>

#define ADDMETHOD_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                          \
    uint32_t funcName##Called = 0u;                                    \
    retType funcName funcParams override {                             \
        funcName##Called++;                                            \
        return funcName##Result;                                       \
    }

#define ADDMETHOD_CONST_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                                \
    mutable uint32_t funcName##Called = 0u;                                  \
    retType funcName funcParams const override {                             \
        funcName##Called++;                                                  \
        return funcName##Result;                                             \
    }

#define ADDMETHOD_NOBASE_VOIDRETURN(funcName, funcParams) \
    uint32_t funcName##Called = 0u;                       \
    void funcName funcParams override {                   \
        funcName##Called++;                               \
    }

#define ADDMETHOD_CONST_NOBASE_VOIDRETURN(funcName, funcParams) \
    void funcName funcParams const override {                   \
    }

#define ADDMETHOD_NOBASE_REFRETURN(funcName, retType, funcParams)        \
    std::remove_reference<retType>::type *funcName##ResultPtr = nullptr; \
    uint32_t funcName##Called = 0u;                                      \
    retType funcName funcParams override {                               \
        UNRECOVERABLE_IF(!funcName##ResultPtr);                          \
        funcName##Called++;                                              \
        return *funcName##ResultPtr;                                     \
    }

#define ADDMETHOD_CONST_NOBASE_REFRETURN(funcName, retType, funcParams)  \
    std::remove_reference<retType>::type *funcName##ResultPtr = nullptr; \
    retType funcName funcParams const override {                         \
        UNRECOVERABLE_IF(!funcName##ResultPtr);                          \
        return *funcName##ResultPtr;                                     \
    }

#define ADDMETHOD(funcName, retType, callBase, defaultReturn, funcParams, invokeParams) \
    retType funcName##Result = defaultReturn;                                           \
    bool funcName##CallBase = callBase;                                                 \
    uint32_t funcName##Called = 0u;                                                     \
    retType funcName funcParams override {                                              \
        funcName##Called++;                                                             \
        if (funcName##CallBase) {                                                       \
            return BaseClass::funcName invokeParams;                                    \
        }                                                                               \
        return funcName##Result;                                                        \
    }

#define ADDMETHOD_CONST(funcName, retType, callBase, defaultReturn, funcParams, invokeParams) \
    retType funcName##Result = defaultReturn;                                                 \
    bool funcName##CallBase = callBase;                                                       \
    mutable uint32_t funcName##Called = 0u;                                                   \
    retType funcName funcParams const override {                                              \
        funcName##Called++;                                                                   \
        if (funcName##CallBase) {                                                             \
            return BaseClass::funcName invokeParams;                                          \
        }                                                                                     \
        return funcName##Result;                                                              \
    }

#define ADDMETHOD_VOIDRETURN(funcName, callBase, funcParams, invokeParams) \
    bool funcName##CallBase = callBase;                                    \
    uint32_t funcName##Called = 0u;                                        \
    void funcName funcParams override {                                    \
        funcName##Called++;                                                \
        if (funcName##CallBase) {                                          \
            BaseClass::funcName invokeParams;                              \
        }                                                                  \
    }
