/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe3 = 6;
inline createReleaseHelperFunctionType releaseHelperFactoryXe3[maxReleaseXe3]{};

EnableReleaseHelperArchitecture<30> enableReleaseHelperArchitecture30(releaseHelperFactoryXe3);

EnableReleaseHelper<ReleaseType::release3000> enablerReleaseHelperPtlH{releaseHelperFactoryXe3[0]};
EnableReleaseHelper<ReleaseType::release3001> enablerReleaseHelperPtlU{releaseHelperFactoryXe3[1]};
EnableReleaseHelper<ReleaseType::release3003> enablerReleaseHelperWcl{releaseHelperFactoryXe3[3]};
EnableReleaseHelper<ReleaseType::release3004> enablerReleaseHelperNvlS{releaseHelperFactoryXe3[4]};
EnableReleaseHelper<ReleaseType::release3005> enablerReleaseHelperNvlU{releaseHelperFactoryXe3[5]};

} // namespace NEO
