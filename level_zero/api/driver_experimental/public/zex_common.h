/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_COMMON_H
#define _ZEX_COMMON_H
#if defined(__cplusplus)
#pragma once
#endif
#include <level_zero/ze_api.h>

#if defined(__cplusplus)
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of command list object
typedef ze_command_list_handle_t zex_command_list_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of event object
typedef ze_event_handle_t zex_event_handle_t;

#define ZEX_BIT(_i) (1 << _i)

typedef uint32_t zex_mem_action_scope_flags_t;
typedef enum _zex_mem_action_scope_flag_t {
    ZEX_MEM_ACTION_SCOPE_FLAG_SUBDEVICE = ZEX_BIT(0),
    ZEX_MEM_ACTION_SCOPE_FLAG_DEVICE = ZEX_BIT(1),
    ZEX_MEM_ACTION_SCOPE_FLAG_HOST = ZEX_BIT(2),
    ZEX_MEM_ACTION_SCOPE_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_mem_action_scope_flag_t;

typedef uint32_t zex_wait_on_mem_action_flags_t;
typedef enum _zex_wait_on_mem_action_flag_t {
    ZEX_WAIT_ON_MEMORY_FLAG_EQUAL = ZEX_BIT(0),
    ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL = ZEX_BIT(1),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN = ZEX_BIT(2),
    ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL = ZEX_BIT(3),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN = ZEX_BIT(4),
    ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL = ZEX_BIT(5),
    ZEX_WAIT_ON_MEMORY_FLAG_FORCE_UINT32 = 0x7fffffff
} zex_wait_on_mem_action_flag_t;

typedef struct _zex_wait_on_mem_desc_t {
    zex_wait_on_mem_action_flags_t actionFlag;
    zex_mem_action_scope_flags_t waitScope;
} zex_wait_on_mem_desc_t;

typedef struct _zex_write_to_mem_desc_t {
    zex_mem_action_scope_flags_t writeScope;
} zex_write_to_mem_desc_t;

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_COMMON_EXTENDED_H
