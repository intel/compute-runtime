/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe = 75;
inline createReleaseHelperFunctionType releaseHelperFactoryXe[maxReleaseXe]{};

EnableReleaseHelperArchitecture<12> enableReleaseHelperArchitecture12{releaseHelperFactoryXe};
EnableReleaseHelper<ReleaseType::release1270> enablerReleaseHelper1270{releaseHelperFactoryXe[70]};
EnableReleaseHelper<ReleaseType::release1271> enablerReleaseHelper1271{releaseHelperFactoryXe[71]};

} // namespace NEO
