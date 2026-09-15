/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

/*This file contains the declarative list of plain OS environment variables read by the driver and ocloc,
  as opposed to the NEO_/NEO_L0_/NEO_OCL_/NEO_OCLOC_-prefixed debug and release variables declared in
  debug_variables_base.inl / release_variables_base.inl. Entries here are looked up by their exact
  external name, with no prefix scanning.*/

/*Scoped variables - explicitly readable in ocloc (S_OCLOC) in addition to the driver (S_RT).*/
DECLARE_RAW_ENV_SCOPED_V(int32_t, EnvCachePersistent, "NEO_CACHE_PERSISTENT", -1, S_RT | S_OCLOC, "Enables persistent compiler cache. -1: default (enabled for the driver, disabled for ocloc), 0: disabled, 1: enabled")
DECLARE_RAW_ENV_SCOPED_V(int64_t, EnvCacheMaxSize, "NEO_CACHE_MAX_SIZE", 1024ll * 1024 * 1024, S_RT | S_OCLOC, "Max size in bytes of persistent compiler cache. 0 means unlimited.")
DECLARE_RAW_ENV_SCOPED_V(std::string, EnvCacheDir, "NEO_CACHE_DIR", std::string(""), S_RT | S_OCLOC, "Directory used for persistent compiler cache.")
DECLARE_RAW_ENV_SCOPED_V(bool, EnvCacheStats, "NEO_CACHE_STATS", false, S_RT | S_OCLOC, "Enables persistent compiler cache statistics.")
DECLARE_RAW_ENV_SCOPED_V(bool, EnvOneapiPvcSendWarWa, "ONEAPI_PVC_SEND_WAR_WA", true, S_RT | S_OCLOC, "Enables the PVC send-WAR workaround. Consumed by the driver and ocloc.")

/*Standard OS environment variables.*/
DECLARE_RAW_ENV_VARIABLE(std::string, EnvHome, "HOME", std::string(""), "Fallback base directory for persistent compiler cache.")
DECLARE_RAW_ENV_VARIABLE(std::string, EnvXdgCacheHome, "XDG_CACHE_HOME", std::string(""), "Base directory for persistent compiler cache.")
DECLARE_RAW_ENV_VARIABLE(std::string, EnvLdLibraryPath, "LD_LIBRARY_PATH", std::string(""), "Additional search paths for optional shared libraries.")
