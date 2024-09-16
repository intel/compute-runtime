/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef TESTS_GEN12LP
#define TGLLP_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_GEN12LP_CORE>::GfxFamily>();
#define TGLLP_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_GEN12LP_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define TGLLP_TYPED_TEST(method)
#define TGLLP_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_XE_HPG_CORE
#define XEHPG_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_XE_HPG_CORE>::GfxFamily>();
#define XEHPG_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_XE_HPG_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define XEHPG_TYPED_TEST(method)
#define XEHPG_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_XE_HPC_CORE
#define XEHPC_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_XE_HPC_CORE>::GfxFamily>();
#define XEHPC_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_XE_HPC_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define XEHPC_TYPED_TEST(method)
#define XEHPC_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_XE2_HPG_CORE
#define XE2HPG_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_XE2_HPG_CORE>::GfxFamily>();
#define XE2HPG_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_XE2_HPG_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define XE2HPG_TYPED_TEST(method)
#define XE2HPG_SUPPORTED_TEST(cmdSetBase) false
#endif
