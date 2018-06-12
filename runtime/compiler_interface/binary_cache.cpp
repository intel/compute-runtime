/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <runtime/compiler_interface/binary_cache.h>
#include <runtime/helpers/aligned_memory.h>
#include <runtime/helpers/file_io.h>
#include <runtime/helpers/hash.h>
#include <runtime/helpers/hw_info.h>
#include <runtime/os_interface/os_inc_base.h>
#include <runtime/program/program.h>

#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace OCLRT {
std::mutex BinaryCache::cacheAccessMtx;

const std::string BinaryCache::getCachedFileName(const HardwareInfo &hwInfo, const ArrayRef<const char> input,
                                                 const ArrayRef<const char> options, const ArrayRef<const char> internalOptions) {
    Hash hash;

    hash.update("----", 4);
    hash.update(&*input.begin(), input.size());
    hash.update("----", 4);
    hash.update(&*options.begin(), options.size());
    hash.update("----", 4);
    hash.update(&*internalOptions.begin(), internalOptions.size());

    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(hwInfo.pPlatform), sizeof(*hwInfo.pPlatform));
    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(hwInfo.pSkuTable), sizeof(*hwInfo.pSkuTable));
    hash.update("----", 4);
    hash.update(reinterpret_cast<const char *>(hwInfo.pWaTable), sizeof(*hwInfo.pWaTable));

    auto res = hash.finish();
    std::stringstream stream;
    stream << std::setfill('0')
           << std::setw(sizeof(res) * 2)
           << std::hex
           << res;
    return stream.str();
}

bool BinaryCache::cacheBinary(const std::string kernelFileHash, const char *pBinary, uint32_t binarySize) {
    if (pBinary == nullptr || binarySize == 0) {
        return false;
    }

    std::string hashFilePath = CL_CACHE_LOCATION;
    hashFilePath.append(Os::fileSeparator);
    hashFilePath.append(kernelFileHash + ".cl_cache");

    std::lock_guard<std::mutex> lock(cacheAccessMtx);
    if (writeDataToFile(
            hashFilePath.c_str(),
            pBinary,
            binarySize) == 0) {
        return false;
    }

    return true;
}

bool BinaryCache::loadCachedBinary(const std::string kernelFileHash, Program &program) {
    void *pBinary = nullptr;
    size_t binarySize = 0;

    std::string hashFilePath = CL_CACHE_LOCATION;
    hashFilePath.append(Os::fileSeparator);
    hashFilePath.append(kernelFileHash + ".cl_cache");

    {
        std::lock_guard<std::mutex> lock(cacheAccessMtx);
        binarySize = loadDataFromFile(hashFilePath.c_str(), pBinary);
    }

    if ((pBinary == nullptr) || (binarySize == 0)) {
        deleteDataReadFromFile(pBinary);
        return false;
    }
    program.storeGenBinary(pBinary, binarySize);

    deleteDataReadFromFile(pBinary);

    return true;
}

} // namespace OCLRT
