/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe = 75;
inline createCompilerReleaseHelperFunctionType compilerReleaseHelperFactoryXe[maxReleaseXe]{};

EnableCompilerReleaseHelperArchitecture<12> enableCompilerReleaseHelperArchitecture12{compilerReleaseHelperFactoryXe};
EnableCompilerReleaseHelper<ReleaseType::release1200> enablerCompilerReleaseHelperTgl{compilerReleaseHelperFactoryXe[0]};
EnableCompilerReleaseHelper<ReleaseType::release1201> enablerCompilerReleaseHelperRkl{compilerReleaseHelperFactoryXe[1]};
EnableCompilerReleaseHelper<ReleaseType::release1202> enablerCompilerReleaseHelperAdlS{compilerReleaseHelperFactoryXe[2]};
EnableCompilerReleaseHelper<ReleaseType::release1203> enablerCompilerReleaseHelperAdlP{compilerReleaseHelperFactoryXe[3]};
EnableCompilerReleaseHelper<ReleaseType::release1204> enablerCompilerReleaseHelperAdlN{compilerReleaseHelperFactoryXe[4]};
EnableCompilerReleaseHelper<ReleaseType::release1210> enablerCompilerReleaseHelperDg1{compilerReleaseHelperFactoryXe[10]};
EnableCompilerReleaseHelper<ReleaseType::release1255> enablerCompilerReleaseHelperDg2G10{compilerReleaseHelperFactoryXe[55]};
EnableCompilerReleaseHelper<ReleaseType::release1256> enablerCompilerReleaseHelperDg2G11{compilerReleaseHelperFactoryXe[56]};
EnableCompilerReleaseHelper<ReleaseType::release1257> enablerCompilerReleaseHelperDg2G12{compilerReleaseHelperFactoryXe[57]};
EnableCompilerReleaseHelper<ReleaseType::release1260> enablerCompilerReleaseHelperPvc{compilerReleaseHelperFactoryXe[60]};
EnableCompilerReleaseHelper<ReleaseType::release1261> enablerCompilerReleaseHelperPvcVg{compilerReleaseHelperFactoryXe[61]};
EnableCompilerReleaseHelper<ReleaseType::release1270> enablerCompilerReleaseHelperMtlU{compilerReleaseHelperFactoryXe[70]};
EnableCompilerReleaseHelper<ReleaseType::release1271> enablerCompilerReleaseHelperMtlH{compilerReleaseHelperFactoryXe[71]};
EnableCompilerReleaseHelper<ReleaseType::release1274> enablerCompilerReleaseHelperArlH{compilerReleaseHelperFactoryXe[74]};

} // namespace NEO
