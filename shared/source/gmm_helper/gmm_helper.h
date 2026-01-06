/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>

namespace NEO {
class GmmClientContext;
struct HardwareInfo;
struct RootDeviceEnvironment;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(const RootDeviceEnvironment &rootDeviceEnvironment);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type) const;
    uint32_t getL1EnabledMOCS() const;
    uint32_t getL3EnabledMOCS() const;
    uint32_t getUncachedMOCS() const;
    void initMocsDefaults();

    static void applyMocsEncryptionBit(uint32_t &index);
    void forceAllResourcesUncached();

    static constexpr uint64_t maxPossiblePitch = (1ull << 31);

    uint64_t canonize(uint64_t address) const;
    uint64_t decanonize(uint64_t address) const;

    uint32_t getAddressWidth() { return addressWidth; };
    void setAddressWidth(uint32_t width) { addressWidth = width; };

    bool isValidCanonicalGpuAddress(uint64_t address);
    void setLocalOnlyAllocationMode(bool value) { localOnlyAllocationModeEnabled = value; }
    bool isLocalOnlyAllocationMode() { return localOnlyAllocationModeEnabled; }

    GmmClientContext *getClientContext() const;

    const RootDeviceEnvironment &getRootDeviceEnvironment() const;
    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(const RootDeviceEnvironment &);

  protected:
    const RootDeviceEnvironment &rootDeviceEnvironment;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    uint32_t addressWidth = 0;
    uint32_t mocsL1Enabled = 0;
    uint32_t mocsL3Enabled = 0;
    uint32_t mocsUncached = 0;
    bool allResourcesUncached = false;
    bool localOnlyAllocationModeEnabled = false;
};
} // namespace NEO
