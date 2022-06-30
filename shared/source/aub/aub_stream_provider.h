/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"

namespace NEO {

class AubStreamProvider {
  public:
    virtual ~AubStreamProvider() = default;

    virtual AubMemDump::AubFileStream *getStream() = 0;
};

class AubFileStreamProvider : public AubStreamProvider {
  public:
    AubMemDump::AubFileStream *getStream() override {
        return &stream;
    };

  protected:
    AubMemDump::AubFileStream stream;
};
} // namespace NEO
