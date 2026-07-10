/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

// clang-format off
using namespace NEO;
using MI_ARB_CHECK                    = GenStruct::MI_ARB_CHECK;
using MI_ATOMIC                       = GenStruct::MI_ATOMIC;
using MI_BATCH_BUFFER_END             = GenStruct::MI_BATCH_BUFFER_END;
using MI_BATCH_BUFFER_START           = GenStruct::MI_BATCH_BUFFER_START;
using MI_LOAD_REGISTER_IMM            = GenStruct::MI_LOAD_REGISTER_IMM;
using MI_LOAD_REGISTER_MEM            = GenStruct::MI_LOAD_REGISTER_MEM;
using MI_STORE_REGISTER_MEM           = GenStruct::MI_STORE_REGISTER_MEM;
using MI_NOOP                         = GenStruct::MI_NOOP;
using PIPE_CONTROL                    = GenStruct::PIPE_CONTROL;
using PIPELINE_SELECT                 = GenStruct::PIPELINE_SELECT;
using STATE_BASE_ADDRESS              = typename StateBaseAddressTypeHelper<GenStruct>::type;
using MI_REPORT_PERF_COUNT            = GenStruct::MI_REPORT_PERF_COUNT;
using MI_MATH                         = GenStruct::MI_MATH;
using MI_LOAD_REGISTER_REG            = GenStruct::MI_LOAD_REGISTER_REG;
using MI_SEMAPHORE_WAIT               = GenGfxFamily::MI_SEMAPHORE_WAIT;
using MI_STORE_DATA_IMM               = GenStruct::MI_STORE_DATA_IMM;
using MI_FLUSH_DW                     = GenStruct::MI_FLUSH_DW;
using MI_USER_INTERRUPT               = GenGfxFamily::MI_USER_INTERRUPT;
using XY_COPY_BLT                     = GenGfxFamily::XY_COPY_BLT;
using XY_BLOCK_COPY_BLT               = GenGfxFamily::XY_BLOCK_COPY_BLT;
using XY_COLOR_BLT                    = GenGfxFamily::XY_COLOR_BLT;
// clang-format on

template <typename SBAType>
SBAType *genSBACast(void *buffer) {
    if constexpr (std::is_same_v<SBAType, SBAPlaceholder>) {
        return nullptr;
    } else {
        return matchCommandHeader<SBAType>(buffer, [](const SBAType &header) {
            return SBAType::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
                   SBAType::COMMAND_SUBTYPE_GFXPIPE_COMMON == header.TheStructure.Common.CommandSubtype &&
                   SBAType::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
                   SBAType::_3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS == header.TheStructure.Common._3DCommandSubOpcode;
        });
    }
}

template <>
STATE_BASE_ADDRESS *genCmdCast<STATE_BASE_ADDRESS *>(void *buffer) {
    return genSBACast<STATE_BASE_ADDRESS>(buffer);
}

template <typename SBAType>
size_t getSBALength(void *cmd) {
    if constexpr (std::is_same_v<SBAType, SBAPlaceholder>) {
        return 0u;
    } else {
        auto pCmd = genCmdCast<SBAType *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
        return 0u;
    }
}

template <>
PIPE_CONTROL *genCmdCast<PIPE_CONTROL *>(void *buffer) {
    return matchCommandHeader<PIPE_CONTROL>(buffer, [](const PIPE_CONTROL &header) {
        return PIPE_CONTROL::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               PIPE_CONTROL::COMMAND_SUBTYPE_GFXPIPE_3D == header.TheStructure.Common.CommandSubtype &&
               PIPE_CONTROL::_3D_COMMAND_OPCODE_PIPE_CONTROL == header.TheStructure.Common._3DCommandOpcode &&
               PIPE_CONTROL::_3D_COMMAND_SUB_OPCODE_PIPE_CONTROL == header.TheStructure.Common._3DCommandSubOpcode;
    });
}

template <>
PIPELINE_SELECT *genCmdCast<PIPELINE_SELECT *>(void *buffer) {
    return matchCommandHeader<PIPELINE_SELECT>(buffer, [](const PIPELINE_SELECT &header) {
        return PIPELINE_SELECT::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               PIPELINE_SELECT::COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW == header.TheStructure.Common.CommandSubtype &&
               PIPELINE_SELECT::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
               PIPELINE_SELECT::_3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT == header.TheStructure.Common._3DCommandSubOpcode;
    });
}

template <>
MI_LOAD_REGISTER_IMM *genCmdCast<MI_LOAD_REGISTER_IMM *>(void *buffer) {
    return matchCommandHeader<MI_LOAD_REGISTER_IMM>(buffer, [](const MI_LOAD_REGISTER_IMM &header) {
        return MI_LOAD_REGISTER_IMM::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_LOAD_REGISTER_IMM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_NOOP *genCmdCast<MI_NOOP *>(void *buffer) {
    return matchCommandHeader<MI_NOOP>(buffer, [](const MI_NOOP &header) {
        return MI_NOOP::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_NOOP::MI_COMMAND_OPCODE_MI_NOOP == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_ATOMIC *genCmdCast<MI_ATOMIC *>(void *buffer) {
    return matchCommandHeader<MI_ATOMIC>(buffer, [](const MI_ATOMIC &header) {
        return MI_ATOMIC::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_ATOMIC::MI_COMMAND_OPCODE_MI_ATOMIC == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_BATCH_BUFFER_END *genCmdCast<MI_BATCH_BUFFER_END *>(void *buffer) {
    return matchCommandHeader<MI_BATCH_BUFFER_END>(buffer, [](const MI_BATCH_BUFFER_END &header) {
        return MI_BATCH_BUFFER_END::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_BATCH_BUFFER_END::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_BATCH_BUFFER_START *genCmdCast<MI_BATCH_BUFFER_START *>(void *buffer) {
    return matchCommandHeader<MI_BATCH_BUFFER_START>(buffer, [](const MI_BATCH_BUFFER_START &header) {
        return MI_BATCH_BUFFER_START::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_BATCH_BUFFER_START::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_LOAD_REGISTER_MEM *genCmdCast<MI_LOAD_REGISTER_MEM *>(void *buffer) {
    return matchCommandHeader<MI_LOAD_REGISTER_MEM>(buffer, [](const MI_LOAD_REGISTER_MEM &header) {
        return MI_LOAD_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_LOAD_REGISTER_MEM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_STORE_REGISTER_MEM *genCmdCast<MI_STORE_REGISTER_MEM *>(void *buffer) {
    return matchCommandHeader<MI_STORE_REGISTER_MEM>(buffer, [](const MI_STORE_REGISTER_MEM &header) {
        return MI_STORE_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_STORE_REGISTER_MEM::MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_REPORT_PERF_COUNT *genCmdCast<MI_REPORT_PERF_COUNT *>(void *buffer) {
    return matchCommandHeader<MI_REPORT_PERF_COUNT>(buffer, [](const MI_REPORT_PERF_COUNT &header) {
        return MI_REPORT_PERF_COUNT::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_REPORT_PERF_COUNT::MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_MATH *genCmdCast<MI_MATH *>(void *buffer) {
    return matchCommandHeader<MI_MATH>(buffer, [](const MI_MATH &header) {
        return MI_MATH::COMMAND_TYPE_MI_COMMAND == header.DW0.BitField.InstructionType &&
               MI_MATH::MI_COMMAND_OPCODE_MI_MATH == header.DW0.BitField.InstructionOpcode;
    });
}

template <>
MI_LOAD_REGISTER_REG *genCmdCast<MI_LOAD_REGISTER_REG *>(void *buffer) {
    return matchCommandHeader<MI_LOAD_REGISTER_REG>(buffer, [](const MI_LOAD_REGISTER_REG &header) {
        return MI_LOAD_REGISTER_REG::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_LOAD_REGISTER_REG::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_SEMAPHORE_WAIT *genCmdCast<MI_SEMAPHORE_WAIT *>(void *buffer) {
    return matchCommandHeader<MI_SEMAPHORE_WAIT>(buffer, [](const MI_SEMAPHORE_WAIT &header) {
        return MI_SEMAPHORE_WAIT::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_SEMAPHORE_WAIT::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_STORE_DATA_IMM *genCmdCast<MI_STORE_DATA_IMM *>(void *buffer) {
    return matchCommandHeader<MI_STORE_DATA_IMM>(buffer, [](const MI_STORE_DATA_IMM &header) {
        return MI_STORE_DATA_IMM::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_STORE_DATA_IMM::MI_COMMAND_OPCODE_MI_STORE_DATA_IMM == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_FLUSH_DW *genCmdCast<MI_FLUSH_DW *>(void *buffer) {
    return matchCommandHeader<MI_FLUSH_DW>(buffer, [](const MI_FLUSH_DW &header) {
        return MI_FLUSH_DW::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_FLUSH_DW::MI_COMMAND_OPCODE_MI_FLUSH_DW == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
XY_COPY_BLT *genCmdCast<XY_COPY_BLT *>(void *buffer) {
    return matchCommandHeader<XY_COPY_BLT>(buffer, [](const XY_COPY_BLT &header) {
        return XY_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == header.TheStructure.Common.InstructionTarget_Opcode &&
               XY_COPY_BLT::CLIENT_2D_PROCESSOR == header.TheStructure.Common.Client;
    });
}

template <>
XY_COLOR_BLT *genCmdCast<XY_COLOR_BLT *>(void *buffer) {
    return matchCommandHeader<XY_COLOR_BLT>(buffer, [](const XY_COLOR_BLT &header) {
        return XY_COLOR_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == header.TheStructure.Common.InstructionTarget_Opcode &&
               XY_COLOR_BLT::CLIENT_2D_PROCESSOR == header.TheStructure.Common.Client;
    });
}

template <>
MI_USER_INTERRUPT *genCmdCast<MI_USER_INTERRUPT *>(void *buffer) {
    return matchCommandHeader<MI_USER_INTERRUPT>(buffer, [](const MI_USER_INTERRUPT &header) {
        return 0 == header.TheStructure.Common.CommandType &&
               MI_USER_INTERRUPT::MI_COMMAND_OPCODE_MI_USER_INTERRUPT == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <>
MI_ARB_CHECK *genCmdCast<MI_ARB_CHECK *>(void *buffer) {
    return matchCommandHeader<MI_ARB_CHECK>(buffer, [](const MI_ARB_CHECK &header) {
        return MI_ARB_CHECK::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_ARB_CHECK::MI_COMMAND_OPCODE_MI_ARB_CHECK == header.TheStructure.Common.MiCommandOpcode;
    });
}

template <class T>
size_t CmdParse<T>::getCommandLength(void *cmd) {
    {
        auto sbaLength = getSBALength<STATE_BASE_ADDRESS>(cmd);
        if (sbaLength != 0) {
            return sbaLength;
        }
    }
    {
        auto pCmd = genCmdCast<PIPE_CONTROL *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_ARB_CHECK *>(cmd);
        if (pCmd) {
            return SIZE32(*pCmd);
        }
    }
    {
        auto pCmd = genCmdCast<MI_ATOMIC *>(cmd);
        if (pCmd) {
            return sizeof(MI_ATOMIC) / sizeof(uint32_t);
        }
    }
    {
        auto pCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
        if (pCmd) {
            return SIZE32(*pCmd);
        }
    }
    {
        auto pCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_NOOP *>(cmd);
        if (pCmd) {
            return SIZE32(*pCmd);
        }
    }
    {
        auto pCmd = genCmdCast<PIPELINE_SELECT *>(cmd);
        if (pCmd) {
            return SIZE32(*pCmd);
        }
    }
    {
        auto pCmd = genCmdCast<MI_REPORT_PERF_COUNT *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_MATH *>(cmd);
        if (pCmd) {
            return pCmd->DW0.BitField.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_STORE_DATA_IMM *>(cmd);
        if (pCmd) {
            return SIZE32(*pCmd);
        }
    }
    {
        auto pCmd = genCmdCast<MI_FLUSH_DW *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<XY_COPY_BLT *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<XY_COLOR_BLT *>(cmd);
        if (pCmd) {
            return pCmd->TheStructure.Common.DwordLength + 2;
        }
    }
    {
        auto pCmd = genCmdCast<MI_USER_INTERRUPT *>(cmd);
        if (pCmd) {
            return sizeof(MI_USER_INTERRUPT) / sizeof(uint32_t);
        }
    }

    auto commandLengthHwSpecific = getCommandLengthHwSpecific(cmd);

    if (commandLengthHwSpecific != 0) {
        return commandLengthHwSpecific;
    }
    return getAdditionalCommandLength(cmd);
}

template <class T>
const char *CmdParse<T>::getCommandName(void *cmd) {
#define RETURN_NAME_IF(CMD_NAME)                \
    if (nullptr != genCmdCast<CMD_NAME *>(cmd)) \
        return #CMD_NAME;

    RETURN_NAME_IF(STATE_BASE_ADDRESS);
    RETURN_NAME_IF(PIPE_CONTROL);
    RETURN_NAME_IF(MI_ARB_CHECK);
    RETURN_NAME_IF(MI_ATOMIC);
    RETURN_NAME_IF(MI_BATCH_BUFFER_END);
    RETURN_NAME_IF(MI_BATCH_BUFFER_START);
    RETURN_NAME_IF(MI_LOAD_REGISTER_IMM);
    RETURN_NAME_IF(MI_LOAD_REGISTER_MEM);
    RETURN_NAME_IF(MI_STORE_REGISTER_MEM);
    RETURN_NAME_IF(MI_NOOP);
    RETURN_NAME_IF(PIPELINE_SELECT);
    RETURN_NAME_IF(MI_REPORT_PERF_COUNT);
    RETURN_NAME_IF(MI_MATH);
    RETURN_NAME_IF(MI_LOAD_REGISTER_REG);
    RETURN_NAME_IF(MI_SEMAPHORE_WAIT);
    RETURN_NAME_IF(MI_STORE_DATA_IMM);
    RETURN_NAME_IF(MI_FLUSH_DW);
    RETURN_NAME_IF(XY_COPY_BLT);
    RETURN_NAME_IF(XY_COLOR_BLT);
    RETURN_NAME_IF(MI_USER_INTERRUPT);

#undef RETURN_NAME_IF

    auto commandNameHwSpecific = getCommandNameHwSpecific(cmd);
    if (strcmp(commandNameHwSpecific, "UNKNOWN") != 0) {
        return commandNameHwSpecific;
    }

    return getAdditionalCommandName(cmd);
}

template <class T>
size_t CmdParse<T>::getAdditionalCommandLength(void *cmd) {
    return 0;
}

template <class T>
const char *CmdParse<T>::getAdditionalCommandName(void *cmd) {
    return "UNKNOWN";
}
