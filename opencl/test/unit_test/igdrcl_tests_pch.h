/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/abort.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/sku_info/sku_info_base.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "test.h"

#include "third_party/opencl_headers/CL/cl.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <string>
#include <vector>
