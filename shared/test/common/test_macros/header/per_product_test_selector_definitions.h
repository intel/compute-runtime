/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef TESTS_GEN8
#define BDW_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_GEN8_CORE>::GfxFamily>();
#define BDW_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_GEN8_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define BDW_TYPED_TEST(method)
#define BDW_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_GEN9
#define SKL_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_GEN9_CORE>::GfxFamily>();
#define SKL_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_GEN9_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define SKL_TYPED_TEST(method)
#define SKL_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_GEN11
#define ICL_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_GEN11_CORE>::GfxFamily>();
#define ICL_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_GEN11_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define ICL_TYPED_TEST(method)
#define ICL_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_GEN12LP
#define TGLLP_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_GEN12LP_CORE>::GfxFamily>();
#define TGLLP_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_GEN12LP_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define TGLLP_TYPED_TEST(method)
#define TGLLP_SUPPORTED_TEST(cmdSetBase) false
#endif
#ifdef TESTS_XE_HP_CORE
#define XEHP_TYPED_TEST(method) method<typename NEO::GfxFamilyMapper<IGFX_XE_HP_CORE>::GfxFamily>();
#define XEHP_SUPPORTED_TEST(cmdSetBase) NEO::GfxFamilyMapper<IGFX_XE_HP_CORE>::GfxFamily::supportsCmdSet(cmdSetBase)
#else
#define XEHP_TYPED_TEST(method)
#define XEHP_SUPPORTED_TEST(cmdSetBase) false
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
