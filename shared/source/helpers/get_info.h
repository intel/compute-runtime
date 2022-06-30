/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "get_info_status.h"

// Need for linux compatibility with memcpy_s
#include "shared/source/helpers/string.h"

#include <cstring>
#include <limits>

namespace GetInfo {

constexpr size_t invalidSourceSize = std::numeric_limits<size_t>::max();

inline GetInfoStatus getInfo(void *destParamValue, size_t destParamValueSize,
                             const void *srcParamValue, size_t srcParamValueSize) {

    if (srcParamValueSize == 0) {
        // Report ok if there is nothing to copy.
        return GetInfoStatus::SUCCESS;
    }

    if ((srcParamValue == nullptr) || (srcParamValueSize == invalidSourceSize)) {
        return GetInfoStatus::INVALID_VALUE;
    }

    if (destParamValue == nullptr) {
        // Report ok if only size is queried.
        return GetInfoStatus::SUCCESS;
    }

    if (destParamValueSize < srcParamValueSize) {
        return GetInfoStatus::INVALID_VALUE;
    }

    // Report ok if we can copy safely.
    memcpy_s(destParamValue, destParamValueSize, srcParamValue, srcParamValueSize);
    return GetInfoStatus::SUCCESS;
}

inline void setParamValueReturnSize(size_t *paramValueSizeRet, size_t newValue, GetInfoStatus getInfoStatus) {
    if ((paramValueSizeRet != nullptr) && (getInfoStatus == GetInfoStatus::SUCCESS)) {
        *paramValueSizeRet = newValue;
    }
}

} // namespace GetInfo

struct GetInfoHelper {
    GetInfoHelper(void *dst, size_t dstSize, size_t *retSize, GetInfoStatus *retVal = nullptr)
        : dst(dst), dstSize(dstSize), retSize(retSize), retVal(retVal) {
    }

    template <typename DataType>
    GetInfoStatus set(const DataType &val) {
        auto errCode = GetInfoStatus::SUCCESS;
        if (retSize != nullptr) {
            *retSize = sizeof(val);
        }
        if (dst != nullptr) {
            if (dstSize >= sizeof(val)) {
                *reinterpret_cast<DataType *>(dst) = val;
            } else {
                errCode = GetInfoStatus::INVALID_VALUE;
            }
        }
        if (retVal)
            *retVal = errCode;
        return errCode;
    }

    template <typename DataType>
    static void set(DataType *dst, DataType val) {
        if (dst) {
            *dst = val;
        }
    }

    void *dst;
    size_t dstSize;
    size_t *retSize;
    GetInfoStatus *retVal;
};

struct ErrorCodeHelper {
    ErrorCodeHelper(int *errcodeRet, int defaultCode)
        : errcodeRet(errcodeRet) {
        set(defaultCode);
    }

    void set(int code) {
        if (errcodeRet != nullptr) {
            *errcodeRet = code;
        }
        localErrcode = code;
    }

    int *errcodeRet;
    int localErrcode;
};

template <typename T>
T getValidParam(T param, T defaultVal = 1, T invalidVal = 0) {
    if (param == invalidVal) {
        return defaultVal;
    }
    return param;
}
