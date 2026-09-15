/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "release_definitions.h"

namespace NEO {

inline constexpr uint32_t maxReleaseXe2 = 5;
inline createCompilerReleaseHelperFunctionType compilerReleaseHelperFactoryXe2[maxReleaseXe2]{};

EnableCompilerReleaseHelperArchitecture<20> enableCompilerReleaseHelperArchitecture20{compilerReleaseHelperFactoryXe2};
EnableCompilerReleaseHelper<ReleaseType::release2001> enablerCompilerReleaseHelperBmgG21{compilerReleaseHelperFactoryXe2[1]};
EnableCompilerReleaseHelper<ReleaseType::release2002> enablerCompilerReleaseHelperBmgG31{compilerReleaseHelperFactoryXe2[2]};
EnableCompilerReleaseHelper<ReleaseType::release2004> enablerCompilerReleaseHelperLnl{compilerReleaseHelperFactoryXe2[4]};

} // namespace NEO
