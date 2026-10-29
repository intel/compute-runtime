/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_VISIT_H
#define _ZEX_VISIT_H
#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>

#include "zex_common.h"

#ifndef ZE_COMMAND_VISIT_EXT_NAME
/// @brief Command Visit Extension Name
#define ZE_COMMAND_VISIT_EXT_NAME "ZE_extension_command_visit"

///////////////////////////////////////////////////////////////////////////////
/// @brief Command Visit Extension version(s)
typedef enum _ze_command_visit_ext_version_t {
    ZE_COMMAND_VISIT_EXT_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_COMMAND_VISIT_EXT_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZE_COMMAND_VISIT_EXT_VERSION_FORCE_UINT32 = 0x7fffffff,       ///< Value marking end of ZE_COMMAND_VISIT_EXT_VERSION_* ENUMs
} ze_command_visit_ext_version_t;

#define ZE_COMMAND_LIST_FLAG_ENABLE_COMMAND_VISIT (static_cast<uint32_t>(ZE_BIT(6)))

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef ZE_VISITOR_CCONV
#if defined(_WIN32)
/// @brief Calling convention for visit extension callbacks
#define ZE_VISITOR_CCONV __cdecl
#else
#define ZE_VISITOR_CCONV
#endif // defined(_WIN32)
#endif // ZE_VISITOR_CCONV

///////////////////////////////////////////////////////////////////////////////
/// @brief Callback function pointer type for hooks executed before and after the
///        default operation during command visit.
typedef void(ZE_VISITOR_CCONV *ze_command_visit_ext_default_op_callback_t)(
    const char *fname,                 ///< [in] function name of the visited API command
    void *userData,                    ///< [in] user-defined data passed from ::ze_command_visit_ext_desc_t
    uint32_t *pNumWaitEvents,          ///< [in,out] pointer to wait event count for the current command
    ze_event_handle_t **pphWaitEvents, ///< [in,out] pointer to wait event list for the current command
    ze_event_handle_t *phSignalEvent   ///< [in,out] pointer to command signal event handle
);

typedef struct _ze_graph_handle_t *ze_graph_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines specialized command visit behavior for a specific Level Zero
///        API function.
typedef struct _ze_concrete_visitor_ext_desc_t {
    ze_structure_type_t stype; ///< [in] type of this structure
    const void *pNext;         ///< [in][optional] must be null or a pointer to an extension-specific
                               ///< structure (i.e. contains stype and pNext).
    const char *fname;         ///< [in] Level Zero API function name for which callback behavior is
                               ///< defined
    void *callback;            ///< [in] callback function pointer; must have the same return type and
                               ///< parameter list as the visited API function named by `fname`, followed
                               ///< by one additional trailing `void*` parameter that receives `userData`
                               ///< from ::ze_command_visit_ext_desc_t, and must use the
                               ///< ::ZE_VISITOR_CCONV calling convention
} ze_concrete_visitor_ext_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines default operation for command visit.
typedef enum _ze_command_visit_ext_default_op_t {
    ZE_COMMAND_VISIT_EXT_DEFAULT_OP_IGNORE = 0,               ///< ignore command if concrete callback is not provided
    ZE_COMMAND_VISIT_EXT_DEFAULT_OP_REAPPEND = 1,             ///< re-append command if concrete callback is not provided
    ZE_COMMAND_VISIT_EXT_DEFAULT_OP_FORCE_UINT32 = 0x7fffffff ///< Value marking end of ZE_COMMAND_VISIT_EXT_DEFAULT_OP_* ENUMs
} ze_command_visit_ext_default_op_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines behavior of command visit.
typedef struct _ze_command_visit_ext_desc_t {
    ze_structure_type_t stype;                                     ///< [in] type of this structure
    const void *pNext;                                             ///< [in][optional] must be null or a pointer to an extension-specific
                                                                   ///< structure (i.e. contains stype and pNext).
    void *userData;                                                ///< [in][optional] user-defined data passed to callbacks
    ze_command_list_handle_t hReappendTargetCommandList;           ///< [in][optional] target command list handle used for default re-append
                                                                   ///< operation
    ze_command_visit_ext_default_op_callback_t beforeDefaultOpClb; ///< [in][optional] callback invoked before `defaultOp` for each command
                                                                   ///< that has no concrete visitor
    ze_command_visit_ext_default_op_t defaultOp;                   ///< [in] default operation to execute for a given command when no concrete
                                                                   ///< callback is provided for it (via ::ze_concrete_visitor_ext_desc_t)
    ze_command_visit_ext_default_op_callback_t afterDefaultOpClb;  ///< [in][optional] callback invoked after `defaultOp` for each command
                                                                   ///< that has no concrete visitor
} ze_command_visit_ext_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Visits all commands in a command list in recorded order.
ze_result_t ZE_APICALL zeCommandListVisitExt(
    ze_command_list_handle_t hCommandList,   ///< [in] handle of command list to visit
    const ze_command_visit_ext_desc_t *pDesc ///< [in] pointer to command visit descriptor
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Visits all commands in a graph in recorded order.
ze_result_t ZE_APICALL zeGraphVisitExt(
    ze_graph_handle_t hGraph,                ///< [in] handle of graph to visit
    const ze_command_visit_ext_desc_t *pDesc ///< [in] pointer to command visit descriptor
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // ZE_COMMAND_VISIT_EXT_NAME
#endif // _ZEX_VISIT_H
