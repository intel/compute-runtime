/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>

namespace NEO {

namespace IoctlToStringHelper {
std::string getIoctlParamStringRemaining(int param) {
    return std::to_string(param);
}
} // namespace IoctlToStringHelper

} // namespace NEO
