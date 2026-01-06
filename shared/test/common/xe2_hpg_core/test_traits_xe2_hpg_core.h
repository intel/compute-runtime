/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_XE2_HPG_CORE> {
    static constexpr bool surfaceStateCompressionParamsSupported = false;
    static constexpr bool clearColorAddressMatcher = false;
    static constexpr bool auxBuiltinsSupported = false;
    static constexpr bool localMemCompressionAubsSupported = true;
    static constexpr bool systemMemCompressionAubsSupported = true;
    static constexpr bool l3ControlSupported = false;
    static constexpr bool forceNonCoherentSupported = false;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = true;
    static constexpr bool iohInSbaSupported = false;
    static constexpr bool auxTranslationSupported = false;
    static constexpr bool fusedEuDispatchSupported = false;
    static constexpr bool numberOfWalkersInCfeStateSupported = false;
    static constexpr bool forceGpuNonCoherent = false;
    static constexpr bool imagesSupported = true;
    static constexpr bool isPipeControlExtendedPriorToNonPipelinedStateCommandSupported = false;
    static constexpr bool largeGrfModeInStateComputeModeSupported = true;
    static constexpr bool heaplessAllowed = false;
    static constexpr bool heaplessRequired = false;
    static constexpr bool bindingTableStateSupported = true;
};
