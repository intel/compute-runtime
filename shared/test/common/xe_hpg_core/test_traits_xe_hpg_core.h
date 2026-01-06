/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_XE_HPG_CORE> {
    static constexpr bool surfaceStateCompressionParamsSupported = true;
    static constexpr bool clearColorAddressMatcher = true;
    static constexpr bool auxBuiltinsSupported = true;
    static constexpr bool localMemCompressionAubsSupported = true;
    static constexpr bool systemMemCompressionAubsSupported = false;
    static constexpr bool l3ControlSupported = true;
    static constexpr bool forceNonCoherentSupported = true;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = false;
    static constexpr bool iohInSbaSupported = false;
    static constexpr bool auxTranslationSupported = true;
    static constexpr bool fusedEuDispatchSupported = true;
    static constexpr bool numberOfWalkersInCfeStateSupported = true;
    static constexpr bool forceGpuNonCoherent = false;
    static constexpr bool isUnTypedDataPortCacheFlushSupported = true;
    static constexpr bool imagesSupported = true;
    static constexpr bool isPipeControlExtendedPriorToNonPipelinedStateCommandSupported = false;
    static constexpr bool largeGrfModeInStateComputeModeSupported = true;
    static constexpr bool heaplessAllowed = false;
    static constexpr bool heaplessRequired = false;
    static constexpr bool bindingTableStateSupported = true;
};
#ifdef TESTS_MTL
#include "shared/test/common/xe_hpg_core/mtl/test_traits_mtl.h"
#endif
#ifdef TESTS_DG2
#include "shared/test/common/xe_hpg_core/dg2/test_traits_dg2.h"
#endif
#ifdef TESTS_ARL
#include "shared/test/common/xe_hpg_core/arl/test_traits_arl.h"
#endif
