/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe3p = 12;
inline createCompilerReleaseHelperFunctionType compilerReleaseHelperFactoryXe3p[maxReleaseXe3p]{};

EnableCompilerReleaseHelperArchitecture<35> enableCompilerReleaseHelperArchitecture35{compilerReleaseHelperFactoryXe3p};
EnableCompilerReleaseHelper<ReleaseType::release3510> enablerCompilerReleaseHelperNvlP{compilerReleaseHelperFactoryXe3p[10]};
EnableCompilerReleaseHelper<ReleaseType::release3511> enablerCompilerReleaseHelperCri{compilerReleaseHelperFactoryXe3p[11]};

} // namespace NEO
