/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/header/per_product_test_selector_definitions.h"

#define FAMILY_SELECTOR(family, methodName)                \
    switch (family) {                                      \
    case IGFX_GEN12LP_CORE:                                \
        TGLLP_TYPED_TEST(methodName)                       \
        break;                                             \
    case IGFX_XE_HPG_CORE:                                 \
        XEHPG_TYPED_TEST(methodName)                       \
        break;                                             \
    case IGFX_XE_HPC_CORE:                                 \
        XEHPC_TYPED_TEST(methodName)                       \
        break;                                             \
    case IGFX_XE2_HPG_CORE:                                \
        XE2HPG_TYPED_TEST(methodName)                      \
        break;                                             \
    default:                                               \
        ASSERT_TRUE((false && "Unknown hardware family")); \
        break;                                             \
    }

#define CALL_IF_SUPPORTED(cmdSetBase, expression)              \
    {                                                          \
        bool supported = false;                                \
        switch (::renderCoreFamily) {                          \
        case IGFX_GEN12LP_CORE:                                \
            supported = TGLLP_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        case IGFX_XE_HPG_CORE:                                 \
            supported = XEHPG_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        case IGFX_XE_HPC_CORE:                                 \
            supported = XEHPC_SUPPORTED_TEST(cmdSetBase);      \
            break;                                             \
        case IGFX_XE2_HPG_CORE:                                \
            supported = XE2HPG_SUPPORTED_TEST(cmdSetBase);     \
            break;                                             \
        default:                                               \
            ASSERT_TRUE((false && "Unknown hardware family")); \
            break;                                             \
        }                                                      \
        if (supported) {                                       \
            expression;                                        \
        }                                                      \
    }
