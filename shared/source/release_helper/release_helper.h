/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"

#include <memory>
namespace NEO {

class ReleaseHelper;
enum class ReleaseType;

inline constexpr uint32_t maxArchitecture = 64;
using createReleaseHelperFunctionType = std::unique_ptr<ReleaseHelper> (*)(HardwareIpVersion hardwareIpVersion);
inline createReleaseHelperFunctionType *releaseHelperFactory[maxArchitecture]{};

class ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion);
    virtual ~ReleaseHelper() = default;

    virtual bool isAdjustWalkOrderAvailable() const = 0;
    virtual bool isMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const = 0;
    virtual bool isProgramAllStateComputeCommandFieldsWARequired() const = 0;
    virtual bool isPrefetchDisablingRequired() const = 0;
    virtual bool isSplitMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isBFloat16ConversionSupported() const = 0;
    virtual int getProductMaxPreferredSlmSize(int preferredEnumValue) const = 0;
    virtual bool getMediaFrequencyTileIndex(uint32_t &tileIndex) const = 0;

  protected:
    ReleaseHelper(HardwareIpVersion hardwareIpVersion) : hardwareIpVersion(hardwareIpVersion) {}
    HardwareIpVersion hardwareIpVersion{};
};

template <ReleaseType releaseType>
class ReleaseHelperHw : public ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion) {
        return std::unique_ptr<ReleaseHelper>(new ReleaseHelperHw<releaseType>{hardwareIpVersion});
    }

    bool isAdjustWalkOrderAvailable() const override;
    bool isMatrixMultiplyAccumulateSupported() const override;
    bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const override;
    bool isProgramAllStateComputeCommandFieldsWARequired() const override;
    bool isPrefetchDisablingRequired() const override;
    bool isSplitMatrixMultiplyAccumulateSupported() const override;
    bool isBFloat16ConversionSupported() const override;
    int getProductMaxPreferredSlmSize(int preferredEnumValue) const override;
    bool getMediaFrequencyTileIndex(uint32_t &tileIndex) const override;

  private:
    ReleaseHelperHw(HardwareIpVersion hardwareIpVersion) : ReleaseHelper(hardwareIpVersion) {}
};

template <uint32_t architecture>
struct EnableReleaseHelperArchitecture {
    EnableReleaseHelperArchitecture(createReleaseHelperFunctionType *releaseTable) {
        releaseHelperFactory[architecture] = releaseTable;
    }
};

template <ReleaseType releaseType>
struct EnableReleaseHelper {
    EnableReleaseHelper(createReleaseHelperFunctionType &releaseTableEntry) {
        using ReleaseHelperType = ReleaseHelperHw<releaseType>;
        releaseTableEntry = ReleaseHelperType::create;
    }
};

} // namespace NEO
