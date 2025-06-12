/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe3 = 4;
inline createReleaseHelperFunctionType releaseHelperFactoryXe3[maxReleaseXe3]{};

EnableReleaseHelperArchitecture<30> enableReleaseHelperArchitecture30(releaseHelperFactoryXe3);

EnableReleaseHelper<ReleaseType::release3000> enablerReleaseHelper3000{releaseHelperFactoryXe3[0]};
EnableReleaseHelper<ReleaseType::release3001> enablerReleaseHelper3001{releaseHelperFactoryXe3[1]};
EnableReleaseHelper<ReleaseType::release3003> enablerReleaseHelper3003{releaseHelperFactoryXe3[3]};

} // namespace NEO
