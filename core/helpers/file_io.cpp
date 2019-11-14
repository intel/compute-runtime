/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "file_io.h"

#include "core/helpers/debug_helpers.h"
#include "core/helpers/stdio.h"

#include <cstring>
#include <new>

std::unique_ptr<char[]> loadDataFromFile(
    const char *filename,
    size_t &retSize) {
    FILE *fp = nullptr;
    size_t nsize = 0;
    std::unique_ptr<char[]> ret;

    DEBUG_BREAK_IF(nullptr == filename);
    // Open the file
    fopen_s(&fp, filename, "rb");
    if (fp) {
        // Allocate a buffer for the file contents
        fseek(fp, 0, SEEK_END);
        nsize = (size_t)ftell(fp);

        fseek(fp, 0, SEEK_SET);

        ret.reset(new (std::nothrow) char[nsize + 1]);

        if (ret) {
            // we initialize to all zeroes before reading in data
            memset(ret.get(), 0x00, nsize + 1);
            auto read = fread(ret.get(), sizeof(unsigned char), nsize, fp);
            DEBUG_BREAK_IF(read != nsize);
            UNUSED_VARIABLE(read);
        } else {
            nsize = 0;
        }

        fclose(fp);
    }

    retSize = nsize;
    return ret;
}

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize) {
    FILE *fp = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(nullptr == pData);
    DEBUG_BREAK_IF(nullptr == filename);

    fopen_s(&fp, filename, "wb");
    if (fp) {
        nsize = fwrite(pData, sizeof(unsigned char), dataSize, fp);
        fclose(fp);
    }

    return nsize;
}

bool fileExists(const std::string &fileName) {
    FILE *pFile = nullptr;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fclose(pFile);
    }
    return pFile != nullptr;
}

bool fileExistsHasSize(const std::string &fileName) {
    FILE *pFile = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fseek(pFile, 0, SEEK_END);
        nsize = (size_t)ftell(pFile);
        fclose(pFile);
    }
    return pFile != nullptr && nsize > 0;
}
