/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/aub_mem_dump/aub_mem_dump.h"

struct MockLrcaHelper : AubMemDump::LrcaHelper {
    mutable uint32_t setContextSaveRestoreFlagsCalled = 0;
    MockLrcaHelper(uint32_t base) : AubMemDump::LrcaHelper(base) {}
    void setContextSaveRestoreFlags(uint32_t &value) const override {
        setContextSaveRestoreFlagsCalled++;
        AubMemDump::LrcaHelper::setContextSaveRestoreFlags(value);
    }
};