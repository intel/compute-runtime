/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"

#include <string>

namespace OCLRT {

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
} // namespace OCLRT
