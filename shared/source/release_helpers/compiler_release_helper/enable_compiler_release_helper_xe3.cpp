/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe3 = 6;
inline createCompilerReleaseHelperFunctionType compilerReleaseHelperFactoryXe3[maxReleaseXe3]{};

EnableCompilerReleaseHelperArchitecture<30> enableCompilerReleaseHelperArchitecture30{compilerReleaseHelperFactoryXe3};
EnableCompilerReleaseHelper<ReleaseType::release3000> enablerCompilerReleaseHelperPtlH{compilerReleaseHelperFactoryXe3[0]};
EnableCompilerReleaseHelper<ReleaseType::release3001> enablerCompilerReleaseHelperPtlU{compilerReleaseHelperFactoryXe3[1]};
EnableCompilerReleaseHelper<ReleaseType::release3003> enablerCompilerReleaseHelperWcl{compilerReleaseHelperFactoryXe3[3]};
EnableCompilerReleaseHelper<ReleaseType::release3004> enablerCompilerReleaseHelperNvlS{compilerReleaseHelperFactoryXe3[4]};
EnableCompilerReleaseHelper<ReleaseType::release3005> enablerCompilerReleaseHelperNvlU{compilerReleaseHelperFactoryXe3[5]};

} // namespace NEO
