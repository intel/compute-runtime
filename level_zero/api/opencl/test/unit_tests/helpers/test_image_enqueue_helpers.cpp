/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/image_enqueue_helpers.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetL0ImageRowPitchTests, given1DArrayWithSlicePitchGreaterThanRowPitchWhenGetL0ImageRowPitchThenSlicePitchIsUsed) {
    constexpr size_t rowPitch = 1316u;
    constexpr size_t slicePitch = 2048u;
    EXPECT_EQ(static_cast<uint32_t>(slicePitch),
              getL0ImageRowPitch(CL_MEM_OBJECT_IMAGE1D_ARRAY, rowPitch, slicePitch));
}

TEST(GetL0ImageRowPitchTests, given1DArrayWithSlicePitchEqualToRowPitchWhenGetL0ImageRowPitchThenRowPitchIsUsed) {
    constexpr size_t pitch = 1316u;
    EXPECT_EQ(static_cast<uint32_t>(pitch),
              getL0ImageRowPitch(CL_MEM_OBJECT_IMAGE1D_ARRAY, pitch, pitch));
}

TEST(GetL0ImageRowPitchTests, given1DArrayWithZeroSlicePitchWhenGetL0ImageRowPitchThenRowPitchIsUsed) {
    constexpr size_t rowPitch = 1316u;
    EXPECT_EQ(static_cast<uint32_t>(rowPitch),
              getL0ImageRowPitch(CL_MEM_OBJECT_IMAGE1D_ARRAY, rowPitch, 0u));
}

TEST(GetL0ImageRowPitchTests, given1DArrayWithSlicePitchSmallerThanRowPitchWhenGetL0ImageRowPitchThenRowPitchIsUsed) {
    constexpr size_t rowPitch = 1316u;
    constexpr size_t slicePitch = 658u;
    EXPECT_EQ(static_cast<uint32_t>(rowPitch),
              getL0ImageRowPitch(CL_MEM_OBJECT_IMAGE1D_ARRAY, rowPitch, slicePitch));
}

TEST(GetL0ImageRowPitchTests, givenNon1DArrayTypesWhenGetL0ImageRowPitchThenRowPitchIsAlwaysUsed) {
    constexpr size_t rowPitch = 1316u;
    constexpr size_t slicePitch = 4096u;

    const cl_mem_object_type nonArrayTypes[] = {
        CL_MEM_OBJECT_IMAGE1D,
        CL_MEM_OBJECT_IMAGE1D_BUFFER,
        CL_MEM_OBJECT_IMAGE2D,
        CL_MEM_OBJECT_IMAGE2D_ARRAY,
        CL_MEM_OBJECT_IMAGE3D,
    };

    for (auto imageType : nonArrayTypes) {
        EXPECT_EQ(static_cast<uint32_t>(rowPitch),
                  getL0ImageRowPitch(imageType, rowPitch, slicePitch));
    }
}

TEST(ResolveHostPitchesForCustomPitchImageTests, givenCustomPitchImageAndZeroRowPitchWhenResolveThenTightRowPitchComputed) {
    constexpr size_t elementSize = 4u;
    const size_t region[3] = {329u, 349u, 1u};
    size_t rowPitch = 0u;
    size_t slicePitch = 0u;

    resolveHostPitchesForCustomPitchImage(true, elementSize, region, rowPitch, slicePitch);

    EXPECT_EQ(329u * elementSize, rowPitch);
    EXPECT_EQ(349u * 329u * elementSize, slicePitch);
}

TEST(ResolveHostPitchesForCustomPitchImageTests, givenCustomPitchImageAndNonZeroRowPitchWhenResolveThenPitchesAreHonored) {
    constexpr size_t elementSize = 4u;
    const size_t region[3] = {329u, 349u, 1u};
    size_t rowPitch = 1328u;
    size_t slicePitch = 1328u * 349u;

    resolveHostPitchesForCustomPitchImage(true, elementSize, region, rowPitch, slicePitch);

    EXPECT_EQ(1328u, rowPitch);
    EXPECT_EQ(1328u * 349u, slicePitch);
}

TEST(ResolveHostPitchesForCustomPitchImageTests, givenCustomPitchImageAndZeroSlicePitchOnlyWhenResolveThenSlicePitchDerivedFromProvidedRowPitch) {
    constexpr size_t elementSize = 4u;
    const size_t region[3] = {329u, 349u, 1u};
    size_t rowPitch = 1328u;
    size_t slicePitch = 0u;

    resolveHostPitchesForCustomPitchImage(true, elementSize, region, rowPitch, slicePitch);

    EXPECT_EQ(1328u, rowPitch);
    EXPECT_EQ(1328u * 349u, slicePitch);
}

TEST(ResolveHostPitchesForCustomPitchImageTests, givenImageWithoutCustomPitchWhenResolveThenPitchesAreLeftUnchanged) {
    constexpr size_t elementSize = 4u;
    const size_t region[3] = {329u, 349u, 1u};
    size_t rowPitch = 0u;
    size_t slicePitch = 0u;

    resolveHostPitchesForCustomPitchImage(false, elementSize, region, rowPitch, slicePitch);

    EXPECT_EQ(0u, rowPitch);
    EXPECT_EQ(0u, slicePitch);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
