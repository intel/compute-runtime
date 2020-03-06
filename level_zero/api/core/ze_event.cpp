/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeEventPoolCreate(
    ze_driver_handle_t hDriver,
    const ze_event_pool_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_event_pool_handle_t *phEventPool) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_EVENT_POOL_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::DriverHandle::fromHandle(hDriver)->createEventPool(desc, numDevices, phDevices, phEventPool);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventPoolDestroy(
    ze_event_pool_handle_t hEventPool) {
    try {
        {
            if (nullptr == hEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::EventPool::fromHandle(hEventPool)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventCreate(
    ze_event_pool_handle_t hEventPool,
    const ze_event_desc_t *desc,
    ze_event_handle_t *phEvent) {
    try {
        {
            if (nullptr == hEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_EVENT_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::eventCreate(hEventPool, desc, phEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventDestroy(
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::eventDestroy(hEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventPoolGetIpcHandle(
    ze_event_pool_handle_t hEventPool,
    ze_ipc_event_pool_handle_t *phIpc) {
    try {
        {
            if (nullptr == hEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phIpc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::EventPool::fromHandle(hEventPool)->getIpcHandle(phIpc);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventPoolOpenIpcHandle(
    ze_driver_handle_t hDriver,
    ze_ipc_event_pool_handle_t hIpc,
    ze_event_pool_handle_t *phEventPool) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::eventPoolOpenIpcHandle(hDriver, hIpc, phEventPool);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventPoolCloseIpcHandle(
    ze_event_pool_handle_t hEventPool) {
    try {
        {
            if (nullptr == hEventPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::EventPool::fromHandle(hEventPool)->closeIpcHandle();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendSignalEvent(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendSignalEvent(hEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendWaitOnEvents(
    ze_command_list_handle_t hCommandList,
    uint32_t numEvents,
    ze_event_handle_t *phEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEvents)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendWaitOnEvents(numEvents, phEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventHostSignal(
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Event::fromHandle(hEvent)->hostSignal();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventHostSynchronize(
    ze_event_handle_t hEvent,
    uint32_t timeout) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Event::fromHandle(hEvent)->hostSynchronize(timeout);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventQueryStatus(
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Event::fromHandle(hEvent)->queryStatus();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendEventReset(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendEventReset(hEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventHostReset(
    ze_event_handle_t hEvent) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Event::fromHandle(hEvent)->reset();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeEventGetTimestamp(
    ze_event_handle_t hEvent,
    ze_event_timestamp_type_t timestampType,
    void *dstptr) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == dstptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Event::fromHandle(hEvent)->getTimestamp(timestampType, dstptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}