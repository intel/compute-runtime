/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"
#include "core/helpers/basic_math.h"
#include "core/memory_manager/memory_constants.h"

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
    uint32_t getMOCS(uint32_t type);

    static constexpr uint64_t maxPossiblePitch = 2147483648;

    static uint64_t canonize(uint64_t address) {
        return static_cast<int64_t>(address << (64 - GmmHelper::addressWidth)) >> (64 - GmmHelper::addressWidth);
    }

    static uint64_t decanonize(uint64_t address) {
        return (address & maxNBitValue(GmmHelper::addressWidth));
    }

    GmmClientContext *getClientContext() const;

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(OSInterface *, HardwareInfo *, decltype(&InitializeGmm), decltype(&GmmAdapterDestroy));

  protected:
    void loadLib();

    static uint32_t addressWidth;
    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<OsLibrary> gmmLib;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    decltype(&InitializeGmm) initGmmFunc;
    decltype(&GmmAdapterDestroy) destroyGmmFunc;
};
} // namespace NEO
