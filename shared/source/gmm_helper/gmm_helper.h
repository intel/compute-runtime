/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {
class GmmClientContext;
class OSInterface;
struct HardwareInfo;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(OSInterface *osInterface, const HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type) const;
    static void applyMocsEncryptionBit(uint32_t &index);
    void forceAllResourcesUncached() { allResourcesUncached = true; };

    static constexpr uint64_t maxPossiblePitch = (1ull << 31);

    uint64_t canonize(uint64_t address);
    uint64_t decanonize(uint64_t address);

    uint32_t getAddressWidth() { return addressWidth; };
    void setAddressWidth(uint32_t width) { addressWidth = width; };

    bool isValidCanonicalGpuAddress(uint64_t address);

    GmmClientContext *getClientContext() const;

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(OSInterface *, HardwareInfo *);

  protected:
    uint32_t addressWidth;
    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    bool allResourcesUncached = false;
};
} // namespace NEO
