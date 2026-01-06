/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_XE_HPC_CORE> {
    static constexpr bool surfaceStateCompressionParamsSupported = true;
    static constexpr bool clearColorAddressMatcher = false;
    static constexpr bool auxBuiltinsSupported = true;
    static constexpr bool localMemCompressionAubsSupported = false;
    static constexpr bool systemMemCompressionAubsSupported = false;
    static constexpr bool l3ControlSupported = false;
    static constexpr bool forceNonCoherentSupported = true;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = true;
    static constexpr bool iohInSbaSupported = false;
    static constexpr bool auxTranslationSupported = true;
    static constexpr bool fusedEuDispatchSupported = false;
    static constexpr bool numberOfWalkersInCfeStateSupported = true;
    static constexpr bool forceGpuNonCoherent = false;
    static constexpr bool isUnTypedDataPortCacheFlushSupported = true;
    static constexpr bool imagesSupported = false;
    static constexpr bool isPipeControlExtendedPriorToNonPipelinedStateCommandSupported = true;
    static constexpr bool largeGrfModeInStateComputeModeSupported = true;
    static constexpr bool heaplessAllowed = false;
    static constexpr bool heaplessRequired = false;
    static constexpr bool bindingTableStateSupported = true;
};
