/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <mutex>
#include "runtime/utilities/arrayref.h"

namespace OCLRT {
struct HardwareInfo;
class Program;

class BinaryCache {
  public:
    static const std::string getCachedFileName(const HardwareInfo &hwInfo, ArrayRef<const char> input,
                                               ArrayRef<const char> options, ArrayRef<const char> internalOptions);
    BinaryCache();
    virtual ~BinaryCache();
    virtual bool cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize);
    virtual bool loadCachedBinary(const std::string kernelFileHash, Program &program);

  protected:
    static std::mutex cacheAccessMtx;
    std::string clCacheLocation;
};
} // namespace OCLRT
