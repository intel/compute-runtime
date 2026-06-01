/*
 * Copyright (C) 2023-2026 Intel Corporation
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
EnableReleaseHelper<ReleaseType::release1200> enablerReleaseHelperTgl{releaseHelperFactoryXe[0]};
EnableReleaseHelper<ReleaseType::release1201> enablerReleaseHelperRkl{releaseHelperFactoryXe[1]};
EnableReleaseHelper<ReleaseType::release1202> enablerReleaseHelperAdlS{releaseHelperFactoryXe[2]};
EnableReleaseHelper<ReleaseType::release1203> enablerReleaseHelperAdlP{releaseHelperFactoryXe[3]};
EnableReleaseHelper<ReleaseType::release1204> enablerReleaseHelperAdlN{releaseHelperFactoryXe[4]};
EnableReleaseHelper<ReleaseType::release1210> enablerReleaseHelperDg1{releaseHelperFactoryXe[10]};
EnableReleaseHelper<ReleaseType::release1255> enablerReleaseHelperDg2G10{releaseHelperFactoryXe[55]};
EnableReleaseHelper<ReleaseType::release1256> enablerReleaseHelperDg2G11{releaseHelperFactoryXe[56]};
EnableReleaseHelper<ReleaseType::release1257> enablerReleaseHelperDg2G12{releaseHelperFactoryXe[57]};
EnableReleaseHelper<ReleaseType::release1260> enablerReleaseHelperPvc{releaseHelperFactoryXe[60]};
EnableReleaseHelper<ReleaseType::release1261> enablerReleaseHelperPvcVg{releaseHelperFactoryXe[61]};
EnableReleaseHelper<ReleaseType::release1270> enablerReleaseHelperMtlU{releaseHelperFactoryXe[70]};
EnableReleaseHelper<ReleaseType::release1271> enablerReleaseHelperMtlH{releaseHelperFactoryXe[71]};
EnableReleaseHelper<ReleaseType::release1274> enablerReleaseHelperArlH{releaseHelperFactoryXe[74]};

} // namespace NEO
