/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "get_info_status.h"

#include <cstring>

// Need for linux compatibility with memcpy_s
#include "shared/source/helpers/string.h"

inline GetInfoStatus getInfo(void *destParamValue, size_t destParamValueSize,
                             const void *srcParamValue, size_t srcParamValueSize) {
    auto retVal = GetInfoStatus::INVALID_VALUE;
    if (srcParamValue && srcParamValueSize) {
        if (!destParamValue && !destParamValueSize) {
            // Report ok if they're looking for size.
            retVal = GetInfoStatus::SUCCESS;
        } else if (destParamValue && destParamValueSize >= srcParamValueSize) {
            // Report ok if we can copy safely
            retVal = GetInfoStatus::SUCCESS;

            memcpy_s(destParamValue, destParamValueSize, srcParamValue, srcParamValueSize);
        } else if (!destParamValue) {
            // Report ok if destParamValue == nullptr and destParamValueSize > 0
            retVal = GetInfoStatus::SUCCESS;
        }
    }

    return retVal;
}

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
