/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == dstptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == srcptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemoryCopy(dstptr, srcptr, size, hEvent, 0, nullptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryFill(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemoryFill(ptr, pattern, patternSize, size, hEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryCopyRegion(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const ze_copy_region_t *dstRegion,
    uint32_t dstPitch,
    uint32_t dstSlicePitch,
    const void *srcptr,
    const ze_copy_region_t *srcRegion,
    uint32_t srcPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == dstptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == dstRegion)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == srcptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == srcRegion)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemoryCopyRegion(dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr, srcRegion, srcPitch, srcSlicePitch, hEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopy(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDstImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hSrcImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendImageCopy(hDstImage, hSrcImage, hEvent, 0, nullptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyRegion(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pDstRegion,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDstImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hSrcImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hEvent, 0, nullptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyToMemory(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == dstptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hSrcImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendImageCopyToMemory(dstptr, hSrcImage, pSrcRegion, hEvent, 0, nullptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyFromMemory(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDstImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == srcptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendImageCopyFromMemory(hDstImage, srcptr, pDstRegion, hEvent, 0, nullptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryPrefetch(
    ze_command_list_handle_t hCommandList,
    const void *ptr,
    size_t size) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemoryPrefetch(ptr, size);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemAdvise(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_advice_t advice) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemAdvise(hDevice, ptr, size, advice);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
