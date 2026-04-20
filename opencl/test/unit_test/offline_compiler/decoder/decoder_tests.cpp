/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/decoder/translate_platform_base.h"

#include "gtest/gtest.h"

#include <array>
#include <utility>

namespace NEO {

TEST(DecoderHelperTest, GivenEmptyPathWhenCallingAddSlashThenNothingIsDone) {
    std::string path;
    addSlash(path);
    EXPECT_TRUE(path.empty());
}

TEST(DecoderHelperTest, GivenPathEndedBySlashWhenCallingAddSlashThenNothingIsDone) {
    std::string path{"./some/path/"};
    addSlash(path);

    EXPECT_EQ("./some/path/", path);
}

TEST(DecoderHelperTest, GivenPathEndedByBackSlashWhenCallingAddSlashThenNothingIsDone) {
    std::string path{".\\some\\path\\"};
    addSlash(path);

    EXPECT_EQ(".\\some\\path\\", path);
}

TEST(DecoderHelperTest, GivenPathWithoutTrailingSeparatorWhenAddSlashThenSlashIsAppended) {
    std::string path = "dump";
    addSlash(path);
    EXPECT_EQ("dump/", path);
}

TEST(DecoderHelperTest, GivenGfxCoreFamilyWhenTranslatingToIgaGenBaseThenExpectedIgaGenBaseIsReturned) {
    constexpr static std::array translations = {
        std::pair{IGFX_GEN12LP_CORE, IGA_XE},
        std::pair{IGFX_XE_HP_CORE, IGA_XE_HP},
        std::pair{IGFX_XE_HPG_CORE, IGA_XE_HPG},
        std::pair{IGFX_XE_HPC_CORE, IGA_XE_HPC},
        std::pair{IGFX_XE2_HPG_CORE, IGA_XE2},
        std::pair{IGFX_XE3_CORE, IGA_XE3},

        std::pair{IGFX_UNKNOWN_CORE, IGA_GEN_INVALID}};

    for (const auto &[input, expectedOutput] : translations) {
        EXPECT_EQ(expectedOutput, translateToIgaGen(input));
    }
}

TEST(DecoderHelperTest, GivenProductFamilyWhenTranslatingToIgaGenBaseThenExpectedIgaGenBaseIsReturned) {
    constexpr static std::array translations = {
        std::pair{IGFX_TIGERLAKE_LP, IGA_XE},
        std::pair{IGFX_ROCKETLAKE, IGA_XE},
        std::pair{IGFX_ALDERLAKE_N, IGA_XE},
        std::pair{IGFX_ALDERLAKE_P, IGA_XE},
        std::pair{IGFX_ALDERLAKE_S, IGA_XE},
        std::pair{IGFX_DG1, IGA_XE},
        std::pair{IGFX_DG2, IGA_XE_HPG},
        std::pair{IGFX_PVC, IGA_XE_HPC},
        std::pair{IGFX_METEORLAKE, IGA_XE_HPG},
        std::pair{IGFX_ARROWLAKE, IGA_XE_HPG},
        std::pair{IGFX_BMG, IGA_XE2},
        std::pair{IGFX_LUNARLAKE, IGA_XE2},
        std::pair{IGFX_PTL, IGA_XE3},
        std::pair{IGFX_NVL_XE3G, IGA_XE3},

        std::pair{IGFX_UNKNOWN, IGA_GEN_INVALID}};

    for (const auto &[input, expectedOutput] : translations) {
        EXPECT_EQ(expectedOutput, translateToIgaGen(input));
    }
}

TEST(DecoderHelperTest, GivenUnknownDeviceNameWhenGetProductFamilyFromDeviceNameThenReturnsIgfxUnknown) {
    EXPECT_EQ(IGFX_UNKNOWN, getProductFamilyFromDeviceName("unknown"));
}

} // namespace NEO
