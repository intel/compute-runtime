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
EnableReleaseHelper<ReleaseType::release1255> enablerReleaseHelper1255{releaseHelperFactoryXe[55]};
EnableReleaseHelper<ReleaseType::release1256> enablerReleaseHelper1256{releaseHelperFactoryXe[56]};
EnableReleaseHelper<ReleaseType::release1257> enablerReleaseHelper1257{releaseHelperFactoryXe[57]};
EnableReleaseHelper<ReleaseType::release1260> enablerReleaseHelper1260{releaseHelperFactoryXe[60]};
EnableReleaseHelper<ReleaseType::release1261> enablerReleaseHelper1261{releaseHelperFactoryXe[61]};
EnableReleaseHelper<ReleaseType::release1270> enablerReleaseHelper1270{releaseHelperFactoryXe[70]};
EnableReleaseHelper<ReleaseType::release1271> enablerReleaseHelper1271{releaseHelperFactoryXe[71]};
EnableReleaseHelper<ReleaseType::release1274> enablerReleaseHelper1274{releaseHelperFactoryXe[74]};

} // namespace NEO
