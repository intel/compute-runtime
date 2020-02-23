/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_constants.h"

#include <memory>

namespace NEO {
class GmmClientContext;
class OsLibrary;
class OSInterface;
struct HardwareInfo;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(OSInterface *osInterface, const HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type) const;

    static constexpr uint64_t maxPossiblePitch = 2147483648;

    static uint64_t canonize(uint64_t address) {
        return static_cast<int64_t>(address << (64 - GmmHelper::addressWidth)) >> (64 - GmmHelper::addressWidth);
    }

    static uint64_t decanonize(uint64_t address) {
        return (address & maxNBitValue(GmmHelper::addressWidth));
    }

    GmmClientContext *getClientContext() const;

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(OSInterface *, HardwareInfo *);

  protected:
    static uint32_t addressWidth;
    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<GmmClientContext> gmmClientContext;
};
} // namespace NEO
