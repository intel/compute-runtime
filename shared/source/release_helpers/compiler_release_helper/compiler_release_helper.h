/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"

#include <memory>

namespace NEO {

class CompilerReleaseHelper;
enum class ReleaseType;
struct HardwareInfo;

inline constexpr uint32_t compilerReleaseMaxArchitecture = 64;
using createCompilerReleaseHelperFunctionType = std::unique_ptr<CompilerReleaseHelper> (*)(HardwareIpVersion hardwareIpVersion);
inline constinit createCompilerReleaseHelperFunctionType *compilerReleaseHelperFactory[compilerReleaseMaxArchitecture]{};

class CompilerReleaseHelper {
  public:
    static std::unique_ptr<CompilerReleaseHelper> create(HardwareIpVersion hardwareIpVersion);
    virtual ~CompilerReleaseHelper() = default;

    bool isAvailableSemaphore64(const HardwareInfo &hwInfo) const;

  protected:
    CompilerReleaseHelper(HardwareIpVersion) {}
};

template <ReleaseType releaseType>
class CompilerReleaseHelperHw : public CompilerReleaseHelper {
  public:
    CompilerReleaseHelperHw(HardwareIpVersion hardwareIpVersion) : CompilerReleaseHelper(hardwareIpVersion) {}
    static std::unique_ptr<CompilerReleaseHelper> create(HardwareIpVersion hardwareIpVersion) {
        return std::make_unique<CompilerReleaseHelperHw<releaseType>>(hardwareIpVersion);
    }
};

template <uint32_t architecture>
struct EnableCompilerReleaseHelperArchitecture {
    EnableCompilerReleaseHelperArchitecture(createCompilerReleaseHelperFunctionType *releaseTable) {
        compilerReleaseHelperFactory[architecture] = releaseTable;
    }
};

template <ReleaseType releaseType>
struct EnableCompilerReleaseHelper {
    EnableCompilerReleaseHelper(createCompilerReleaseHelperFunctionType &releaseTableEntry) {
        releaseTableEntry = CompilerReleaseHelperHw<releaseType>::create;
    }
};

} // namespace NEO
