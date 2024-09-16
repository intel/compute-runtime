/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test_base.h"

#include "gtest/gtest.h"

#ifdef TESTS_XE_HPG_CORE
#define XE_HPG_CORETEST_F(test_fixture, test_name) GENTEST_F(IGFX_XE_HPG_CORE, test_fixture, test_name)
#define XE_HPG_CORETEST_P(test_fixture, test_name) GENTEST_P(IGFX_XE_HPG_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_XE_HPC_CORE
#define XE_HPC_CORETEST_F(test_fixture, test_name) GENTEST_F(IGFX_XE_HPC_CORE, test_fixture, test_name)
#define XE_HPC_CORETEST_P(test_fixture, test_name) GENTEST_P(IGFX_XE_HPC_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_GEN12LP
#define GEN12LPTEST_F(test_fixture, test_name) GENTEST_F(IGFX_GEN12LP_CORE, test_fixture, test_name)
#define GEN12LPTEST_P(test_fixture, test_name) GENTEST_P(IGFX_GEN12LP_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_XE2_HPG_CORE
#define XE2_HPG_CORETEST_F(test_fixture, test_name) GENTEST_F(IGFX_XE2_HPG_CORE, test_fixture, test_name)
#define XE2_HPG_CORETEST_P(test_fixture, test_name) GENTEST_P(IGFX_XE2_HPG_CORE, test_fixture, test_name)
#endif

#ifdef TESTS_TGLLP
#define TGLLPTEST_F(test_fixture, test_name)                         \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_TIGERLAKE_LP)
#define TGLLPTEST_P(test_suite_name, test_name)   \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_TIGERLAKE_LP)
#endif
#ifdef TESTS_DG1
#define DG1TEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_DG1)
#define DG1TEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_DG1)
#endif
#ifdef TESTS_RKL
#define RKLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_ROCKETLAKE)
#define RKLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_ROCKETLAKE)
#endif
#ifdef TESTS_ADLS
#define ADLSTEST_F(test_fixture, test_name)                          \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_ALDERLAKE_S)
#define ADLSTEST_P(test_suite_name, test_name)    \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_ALDERLAKE_S)
#endif
#ifdef TESTS_ADLP
#define ADLPTEST_F(test_fixture, test_name)                          \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_ALDERLAKE_P)
#define ADLPTEST_P(test_suite_name, test_name)    \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_ALDERLAKE_P)
#endif

#ifdef TESTS_ADLN
#define ADLNTEST_F(test_fixture, test_name)                          \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN12LP_CORE, IGFX_ALDERLAKE_N)
#define ADLNTEST_P(test_suite_name, test_name)    \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN12LP_CORE,          \
                      IGFX_ALDERLAKE_N)
#endif

#ifdef TESTS_ARL
#define ARLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE_HPG_CORE, IGFX_ARROWLAKE)
#define ARLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE_HPG_CORE,           \
                      IGFX_ARROWLAKE)
#endif

#ifdef TESTS_MTL
#define MTLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE_HPG_CORE, IGFX_METEORLAKE)
#define MTLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE_HPG_CORE,           \
                      IGFX_METEORLAKE)
#endif

#ifdef TESTS_DG2
#define DG2TEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE_HPG_CORE, IGFX_DG2)
#define DG2TEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE_HPG_CORE,           \
                      IGFX_DG2)
#endif

#ifdef TESTS_PVC
#define PVCTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE_HPC_CORE, IGFX_PVC)
#define PVCTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE_HPC_CORE,           \
                      IGFX_PVC)
#endif
#ifdef TESTS_BMG
#define BMGTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE2_HPG_CORE, IGFX_BMG)
#define BMGTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE2_HPG_CORE,          \
                      IGFX_BMG)
#endif
#ifdef TESTS_LNL
#define LNLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE2_HPG_CORE, IGFX_LUNARLAKE)
#define LNLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE2_HPG_CORE,          \
                      IGFX_LUNARLAKE)
#endif
