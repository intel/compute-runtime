/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define ADDMETHOD_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                          \
    uint32_t funcName##Called = 0u;                                    \
    retType funcName funcParams override {                             \
        funcName##Called++;                                            \
        return funcName##Result;                                       \
    }

#define ADDMETHOD_CONST_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                                \
    retType funcName funcParams const override {                             \
        return funcName##Result;                                             \
    }

#define ADDMETHOD_NOBASE_VOIDRETURN(funcName, funcParams) \
    uint32_t funcName##Called = 0u;                       \
    void funcName funcParams override {                   \
        funcName##Called++;                               \
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
