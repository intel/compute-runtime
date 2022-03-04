/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/header/per_product_test_selector_definitions.h"

#define FAMILY_SELECTOR(family, methodName)                \
    switch (family) {                                      \
    case IGFX_GEN8_CORE:                                   \
        BDW_TYPED_TEST(methodName)                         \
        break;                                             \
    case IGFX_GEN9_CORE:                                   \
        SKL_TYPED_TEST(methodName)                         \
        break;                                             \
    case IGFX_GEN11_CORE:                                  \
        ICL_TYPED_TEST(methodName)                         \
        break;                                             \
    case IGFX_GEN12LP_CORE:                                \
        TGLLP_TYPED_TEST(methodName)                       \
        break;                                             \
    case IGFX_XE_HP_CORE:                                  \
        XEHP_TYPED_TEST(methodName)                        \
        break;                                             \
    case IGFX_XE_HPG_CORE:                                 \
        XEHPG_TYPED_TEST(methodName)                       \
        break;                                             \
    case IGFX_XE_HPC_CORE:                                 \
        XEHPC_TYPED_TEST(methodName)                       \
        break;                                             \
    default:                                               \
        ASSERT_TRUE((false && "Unknown hardware family")); \
        break;                                             \
    }

#define CALL_IF_SUPPORTED(cmdSetBase, expression)              \
    {                                                          \
        bool supported = false;                                \
        switch (::renderCoreFamily) {                          \
        case IGFX_GEN8_CORE:                                   \
            supported = BDW_SUPPORTED_TEST(cmdSetBase);        \
            break;                                             \
        case IGFX_GEN9_CORE:                                   \
            supported = SKL_SUPPORTED_TEST(cmdSetBase);        \
            break;                                             \
        case IGFX_GEN11_CORE:                                  \
            supported = ICL_SUPPORTED_TEST(cmdSetBase);        \
            break;                                             \
        case IGFX_GEN12LP_CORE:                                \
            supported = TGLLP_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        case IGFX_XE_HP_CORE:                                  \
            supported = XEHP_SUPPORTED_TEST(cmdSetBase);       \
            break;                                             \
        case IGFX_XE_HPG_CORE:                                 \
            supported = XEHPG_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        case IGFX_XE_HPC_CORE:                                 \
            supported = XEHPC_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        default:                                               \
            ASSERT_TRUE((false && "Unknown hardware family")); \
            break;                                             \
        }                                                      \
        if (supported) {                                       \
            expression;                                        \
        }                                                      \
    }
