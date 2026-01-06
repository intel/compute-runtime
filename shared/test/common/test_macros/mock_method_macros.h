/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"

#include <type_traits>

#define ADDMETHOD_NOBASE_OVERLOAD(funcName, overloadId, retType, defaultReturn, funcParams) \
    retType funcName##overloadId##Result = defaultReturn;                                   \
    uint32_t funcName##overloadId##Called = 0u;                                             \
    retType funcName funcParams override {                                                  \
        funcName##overloadId##Called++;                                                     \
        return funcName##overloadId##Result;                                                \
    }

#define ADDMETHOD_CONST_NOBASE_OVERLOAD(funcName, overloadId, retType, defaultReturn, funcParams) \
    retType funcName##overloadId##Result = defaultReturn;                                         \
    mutable uint32_t funcName##overloadId##Called = 0u;                                           \
    retType funcName funcParams const override {                                                  \
        funcName##overloadId##Called++;                                                           \
        return funcName##overloadId##Result;                                                      \
    }

#define ADDMETHOD_NOBASE_VOIDRETURN_OVERLOAD(funcName, overloadId, funcParams) \
    uint32_t funcName##overloadId##Called = 0u;                                \
    void funcName funcParams override {                                        \
        funcName##overloadId##Called++;                                        \
    }

#define ADDMETHOD_NOBASE_REFRETURN_OVERLOAD(funcName, overloadId, retType, funcParams) \
    std::remove_reference<retType>::type *funcName##overloadId##ResultPtr = nullptr;   \
    uint32_t funcName##overloadId##Called = 0u;                                        \
    retType funcName funcParams override {                                             \
        UNRECOVERABLE_IF(!funcName##overloadId##ResultPtr);                            \
        funcName##overloadId##Called++;                                                \
        return *funcName##overloadId##ResultPtr;                                       \
    }

#define ADDMETHOD_CONST_NOBASE_REFRETURN_OVERLOAD(funcName, overloadId, retType, funcParams) \
    std::remove_reference<retType>::type *funcName##overloadId##ResultPtr = nullptr;         \
    retType funcName funcParams const override {                                             \
        UNRECOVERABLE_IF(!funcName##overloadId##ResultPtr);                                  \
        return *funcName##overloadId##ResultPtr;                                             \
    }

#define ADDMETHOD_OVERLOAD(funcName, overloadId, retType, callBase, defaultReturn, funcParams, invokeParams) \
    retType funcName##overloadId##Result = defaultReturn;                                                    \
    bool funcName##overloadId##CallBase = callBase;                                                          \
    uint32_t funcName##overloadId##Called = 0u;                                                              \
    retType funcName funcParams override {                                                                   \
        funcName##overloadId##Called++;                                                                      \
        if (funcName##overloadId##CallBase) {                                                                \
            return BaseClass::funcName invokeParams;                                                         \
        }                                                                                                    \
        return funcName##overloadId##Result;                                                                 \
    }

#define ADDMETHOD_CONST_OVERLOAD(funcName, overloadId, retType, callBase, defaultReturn, funcParams, invokeParams) \
    retType funcName##overloadId##Result = defaultReturn;                                                          \
    bool funcName##overloadId##CallBase = callBase;                                                                \
    mutable uint32_t funcName##overloadId##Called = 0u;                                                            \
    retType funcName funcParams const override {                                                                   \
        funcName##overloadId##Called++;                                                                            \
        if (funcName##overloadId##CallBase) {                                                                      \
            return BaseClass::funcName invokeParams;                                                               \
        }                                                                                                          \
        return funcName##overloadId##Result;                                                                       \
    }

#define ADDMETHOD_VOIDRETURN_OVERLOAD(funcName, overloadId, callBase, funcParams, invokeParams) \
    bool funcName##overloadId##CallBase = callBase;                                             \
    uint32_t funcName##overloadId##Called = 0u;                                                 \
    void funcName funcParams override {                                                         \
        funcName##overloadId##Called++;                                                         \
        if (funcName##overloadId##CallBase) {                                                   \
            BaseClass::funcName invokeParams;                                                   \
        }                                                                                       \
    }

#define ADDMETHOD_NOBASE(funcName, retType, defaultReturn, funcParams) \
    ADDMETHOD_NOBASE_OVERLOAD(funcName, , retType, defaultReturn, funcParams)

#define ADDMETHOD_CONST_NOBASE(funcName, retType, defaultReturn, funcParams) \
    ADDMETHOD_CONST_NOBASE_OVERLOAD(funcName, , retType, defaultReturn, funcParams)

#define ADDMETHOD_NOBASE_VOIDRETURN(funcName, funcParams) \
    ADDMETHOD_NOBASE_VOIDRETURN_OVERLOAD(funcName, , funcParams)

#define ADDMETHOD_CONST_NOBASE_VOIDRETURN(funcName, funcParams) \
    void funcName funcParams const override {                   \
    }

#define ADDMETHOD_NOBASE_REFRETURN(funcName, retType, funcParams) \
    ADDMETHOD_NOBASE_REFRETURN_OVERLOAD(funcName, , retType, funcParams)

#define ADDMETHOD_CONST_NOBASE_REFRETURN(funcName, retType, funcParams) \
    ADDMETHOD_CONST_NOBASE_REFRETURN_OVERLOAD(funcName, , retType, funcParams)

#define ADDMETHOD(funcName, retType, callBase, defaultReturn, funcParams, invokeParams) \
    ADDMETHOD_OVERLOAD(funcName, , retType, callBase, defaultReturn, funcParams, invokeParams)

#define ADDMETHOD_CONST(funcName, retType, callBase, defaultReturn, funcParams, invokeParams) \
    ADDMETHOD_CONST_OVERLOAD(funcName, , retType, callBase, defaultReturn, funcParams, invokeParams)

#define ADDMETHOD_VOIDRETURN(funcName, callBase, funcParams, invokeParams) \
    ADDMETHOD_VOIDRETURN_OVERLOAD(funcName, , callBase, funcParams, invokeParams)
