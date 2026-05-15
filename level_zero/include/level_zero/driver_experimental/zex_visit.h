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
/// @brief Command List Visit Extension Name
#define ZE_COMMAND_VISIT_EXT_NAME "ZE_extension_command_visit"

///////////////////////////////////////////////////////////////////////////////
/// @brief Command List Visit Extension version(s)
typedef enum _ze_command_visit_ext_version_t {
    ZE_COMMAND_VISIT_EXT_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZE_COMMAND_VISIT_EXT_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZE_COMMAND_VISIT_EXT_VERSION_FORCE_UINT32 = 0x7fffffff,       ///< Value marking end of ZE_COMMAND_VISIT_EXT_VERSION_* ENUMs
} ze_command_visit_ext_version_t;

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32
#define VISITOR_CCONV __cdecl
#else
#define VISITOR_CCONV
#endif

typedef void(VISITOR_CCONV *ze_visit_ext_default_op_callback_t)(const char *fname, void *userData, uint32_t *numWaitEvents, ze_event_handle_t **phWaitEvents, ze_event_handle_t *hSignalEvent);
typedef struct _ze_graph_handle_t *ze_graph_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines specialized behavior of zeCommandListVisitExt visit operation for given L0 API function
///        One or more instances of this structure can be chained in pNext of ze_visit_ext_desc_t to define behavior for multiple L0 API functions
typedef struct _ze_concrete_visitor_ext_desc_t {
    ze_structure_type_ext_t stype;
    const void *pNext;

    const char *fname; ///< [in] L0 API function name for which the behavior is defined, for example "zeCommandListAppendMemoryCopyWithParameters"
    void *callback;    ///< [in] The implementation of the behavior, the type of the function pointer must match the signature of the L0 API function for which the behavior is defined with one extra parameter at the end - void*
                       ///       Note : calling convention must match VISITOR_CCONV
} ze_concrete_visitor_ext_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines default operation for visit call
typedef enum _ze_visit_ext_default_op_t {
    ZE_VISIT_EXT_DEFAULT_OP_IGNORE = 0,   // if concrete visitor callback is not provided, ignore the command
    ZE_VISIT_EXT_DEFAULT_OP_REAPPEND = 1, // if concrete visitor callback is not provided, reappend the command
} ze_visit_ext_default_op_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines behavior of visit operations
typedef struct _ze_visit_ext_desc_t {
    ze_structure_type_ext_t stype;
    const void *pNext;

    void *userData; // optional user data to be passed to callbacks

    ze_command_list_handle_t hReappendTargetCmdList; ///< [in][optional]  target commandlist for ZE_VISIT_EXT_DEFAULT_OP_REAPPEND
                                                     ///                  if visiting a graph, then hReappendTargetCmdList will be used only for root level recording (i.e. not used in forks)
                                                     ///                  If hReappendTargetCmdList is not set then reappends the command to the same commandlist as originally recorded.

    ze_visit_ext_default_op_callback_t beforeDefaultOpClb; ///< [in][optional] callback to be called before executing default operation, can be null if no action is needed before default operation
    ze_visit_ext_default_op_t defaultOp;                   ///< [in]           default operation to be taken if concrete visitor callback is not provided
    ze_visit_ext_default_op_callback_t afterDefaultOpClb;  ///< [in][optional] callback to be called after executing default operation, can be null if no action is needed after default operation
} ze_visit_ext_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Visits all commands in a commandlist in order of recording operations
ze_result_t ZE_APICALL zeCommandListVisitExt(ze_command_list_handle_t cmdlist,
                                             const ze_visit_ext_desc_t *desc);

///////////////////////////////////////////////////////////////////////////////
/// @brief Visits all commands in a graph in order of recording operations
ze_result_t ZE_APICALL zeGraphVisitExt(ze_graph_handle_t graph,
                                       const ze_visit_ext_desc_t *desc);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // ZE_COMMAND_VISIT_EXT_NAME
#endif // _ZEX_VISIT_H
