/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe2 = 5;
inline createReleaseHelperFunctionType releaseHelperFactoryXe2[maxReleaseXe2]{};

EnableReleaseHelperArchitecture<20> enableReleaseHelperArchitecture20(releaseHelperFactoryXe2);
EnableReleaseHelper<ReleaseType::release2001> enablerReleaseHelper2001{releaseHelperFactoryXe2[1]};
EnableReleaseHelper<ReleaseType::release2002> enablerReleaseHelper2002{releaseHelperFactoryXe2[2]};
EnableReleaseHelper<ReleaseType::release2004> enablerReleaseHelper2004{releaseHelperFactoryXe2[4]};
} // namespace NEO
