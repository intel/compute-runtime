/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include <vector>

namespace NEO {

class KmtAdapterFactory : public AdapterFactory {
  public:
    explicit KmtAdapterFactory(const Gdi &gdi) : gdi(gdi) {}

    bool createSnapshotOfAvailableAdapters() override;

    bool isSupported() override;

    uint32_t getNumAdaptersInSnapshot() override;

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override;

  protected:
    const Gdi &gdi;
    std::vector<AdapterDesc> adaptersInSnapshot;
};

} // namespace NEO
