/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstring>

// Need for linux compatibility with memcpy_s
#include "core/helpers/string.h"

inline cl_int getInfo(void *destParamValue, size_t destParamValueSize,
                      const void *srcParamValue, size_t srcParamValueSize) {
    auto retVal = CL_INVALID_VALUE;
    if (srcParamValue && srcParamValueSize) {
        if (!destParamValue && !destParamValueSize) {
            // Report ok if they're looking for size.
            retVal = CL_SUCCESS;
        } else if (destParamValue && destParamValueSize >= srcParamValueSize) {
            // Report ok if we can copy safely
            retVal = CL_SUCCESS;

            memcpy_s(destParamValue, destParamValueSize, srcParamValue, srcParamValueSize);
        } else if (!destParamValue) {
            // Report ok if destParamValue == nullptr and destParamValueSize > 0
            retVal = CL_SUCCESS;
        }
    }

    return retVal;
}

struct GetInfoHelper {
    GetInfoHelper(void *dst, size_t dstSize, size_t *retSize, cl_int *retVal = nullptr)
        : dst(dst), dstSize(dstSize), retSize(retSize), retVal(retVal) {
    }

    template <typename DataType>
    cl_int set(const DataType &val) {
        cl_int errCode = CL_SUCCESS;
        if (retSize != nullptr) {
            *retSize = sizeof(val);
        }
        if (dst != nullptr) {
            if (dstSize >= sizeof(val)) {
                *reinterpret_cast<DataType *>(dst) = val;
            } else {
                errCode = CL_INVALID_VALUE;
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
    cl_int *retVal;
};

struct ErrorCodeHelper {
    ErrorCodeHelper(cl_int *errcodeRet, cl_int defaultCode)
        : errcodeRet(errcodeRet) {
        set(defaultCode);
    }

    void set(cl_int code) {
        if (errcodeRet != nullptr) {
            *errcodeRet = code;
        }
        localErrcode = code;
    }

    cl_int *errcodeRet;
    cl_int localErrcode;
};

template <typename T>
T getValidParam(T param, T defaultVal = 1, T invalidVal = 0) {
    if (param == invalidVal) {
        return defaultVal;
    }
    return param;
}
