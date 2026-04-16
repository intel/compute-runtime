/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "release_definitions.h"

namespace NEO {
inline constexpr uint32_t maxReleaseXe3p = 12;
inline createReleaseHelperFunctionType releaseHelperFactoryXe3p[maxReleaseXe3p]{};

EnableReleaseHelperArchitecture<35> enableReleaseHelperArchitecture35(releaseHelperFactoryXe3p);
EnableReleaseHelper<ReleaseType::release3510> enablerReleaseHelper3510{releaseHelperFactoryXe3p[10]};
EnableReleaseHelper<ReleaseType::release3511> enablerReleaseHelper3511{releaseHelperFactoryXe3p[11]};
} // namespace NEO
