/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <map>
#include <string_view>

namespace L0 {
namespace Sysman {

const std::map<std::string_view, zes_engine_type_flag_t> sysfsEngineMapToLevel0EngineType = {
    {"rcs", ZES_ENGINE_TYPE_FLAG_RENDER},
    {"ccs", ZES_ENGINE_TYPE_FLAG_COMPUTE},
    {"bcs", ZES_ENGINE_TYPE_FLAG_DMA},
    {"vcs", ZES_ENGINE_TYPE_FLAG_MEDIA},
    {"vecs", ZES_ENGINE_TYPE_FLAG_OTHER}};

} // namespace Sysman
} // namespace L0
