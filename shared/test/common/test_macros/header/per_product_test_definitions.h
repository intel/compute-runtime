/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test_base.h"

#include "gtest/gtest.h"

#ifdef TESTS_GEN8
#define GEN8TEST_F(test_fixture, test_name) GENTEST_F(IGFX_GEN8_CORE, test_fixture, test_name)
#define GEN8TEST_P(test_fixture, test_name) GENTEST_P(IGFX_GEN8_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_GEN9
#define GEN9TEST_F(test_fixture, test_name) GENTEST_F(IGFX_GEN9_CORE, test_fixture, test_name)
#define GEN9TEST_P(test_fixture, test_name) GENTEST_P(IGFX_GEN9_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_GEN11
#define GEN11TEST_F(test_fixture, test_name) GENTEST_F(IGFX_GEN11_CORE, test_fixture, test_name)
#define GEN11TEST_P(test_fixture, test_name) GENTEST_P(IGFX_GEN11_CORE, test_fixture, test_name)
#endif
#ifdef TESTS_XE_HP_CORE
#define XE_HP_CORE_TEST_F(test_fixture, test_name) GENTEST_F(IGFX_XE_HP_CORE, test_fixture, test_name)
#define XE_HP_CORE_TEST_P(test_fixture, test_name) GENTEST_P(IGFX_XE_HP_CORE, test_fixture, test_name)
#endif
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
#ifdef TESTS_GEN8
#define BDWTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN8_CORE, IGFX_BROADWELL)
#define BDWTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN8_CORE,             \
                      IGFX_BROADWELL)
#endif

#ifdef TESTS_SKL
#define SKLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN9_CORE, IGFX_SKYLAKE)
#define SKLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN9_CORE,             \
                      IGFX_SKYLAKE)
#endif
#ifdef TESTS_KBL
#define KBLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN9_CORE, IGFX_KABYLAKE)
#define KBLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN9_CORE,             \
                      IGFX_KABYLAKE)
#endif
#ifdef TESTS_CFL
#define CFLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN9_CORE, IGFX_COFFEELAKE)
#define CFLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN9_CORE,             \
                      IGFX_COFFEELAKE)
#endif

#ifdef TESTS_GLK
#define GLKTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN9_CORE, IGFX_GEMINILAKE)
#define GLKTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN9_CORE,             \
                      IGFX_GEMINILAKE)
#endif
#ifdef TESTS_BXT
#define BXTTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN9_CORE, IGFX_BROXTON)
#define BXTTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN9_CORE,             \
                      IGFX_BROXTON)
#endif

#ifdef TESTS_ICLLP
#define ICLLPTEST_F(test_fixture, test_name)                         \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN11_CORE, IGFX_ICELAKE_LP)
#define ICLLPTEST_P(test_suite_name, test_name)   \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN11_CORE,            \
                      IGFX_ICELAKE_LP)
#endif
#ifdef TESTS_LKF
#define LKFTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN11_CORE, IGFX_LAKEFIELD)
#define LKFTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN11_CORE,            \
                      IGFX_LAKEFIELD)
#endif
#ifdef TESTS_EHL
#define EHLTEST_F(test_fixture, test_name)                           \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_GEN11_CORE, IGFX_ELKHARTLAKE)
#define EHLTEST_P(test_suite_name, test_name)     \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_GEN11_CORE,            \
                      IGFX_ELKHARTLAKE)
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

#ifdef TESTS_XE_HP_SDV
#define XEHPTEST_F(test_fixture, test_name)                          \
    FAMILYTEST_TEST_(test_fixture, test_name, test_fixture,          \
                     ::testing::internal::GetTypeId<test_fixture>(), \
                     IGFX_XE_HP_CORE, IGFX_XE_HP_SDV)
#define XEHPTEST_P(test_suite_name, test_name)    \
    FAMILYTEST_TEST_P(test_suite_name, test_name, \
                      IGFX_XE_HP_CORE,            \
                      IGFX_XE_HP_SDV)
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
