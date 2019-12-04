/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"
#include "core/memory_manager/memory_constants.h"

#include <memory>

namespace NEO {
class GmmClientContext;
class OsLibrary;
struct HardwareInfo;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(const HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type);

    static constexpr uint64_t maxPossiblePitch = 2147483648;

    template <uint8_t addressWidth = 48>
    static uint64_t canonize(uint64_t address) {
        return ((int64_t)(address << (64 - addressWidth))) >> (64 - addressWidth);
    }

    template <uint8_t addressWidth = 48>
    static uint64_t decanonize(uint64_t address) {
        return (address & maxNBitValue(addressWidth));
    }

    GmmClientContext *getClientContext() const;

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(HardwareInfo *, decltype(&InitializeGmm), decltype(&GmmDestroy));

  protected:
    void loadLib();

    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<OsLibrary> gmmLib;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    decltype(&InitializeGmm) initGmmFunc;
    decltype(&GmmDestroy) destroyGmmFunc;
};
} // namespace NEO
