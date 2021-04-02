/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/software_tags.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"

namespace NEO {
namespace SWTags {

void BXMLHeapInfo::bxml(std::ostream &os) {
    os << "<Structure Name=\"SWTAG_BXML_HEAP_INFO\" Source=\"Driver\" Project=\"All\">\n";
    os << "  <Description>BXML heap info. This will always be at offset 0.</Description>\n";
    os << "  <DWord Name=\"0\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"MagicNumber\" Format=\"U32\">\n";
    os << "      <Description>This is the target of the MI_STORE_DATA_IMM that specifies where the heap exists. This value will always be 0xDEB06D0C.</Description>\n";
    os << "      <ValidValue Value=\"DEB06D0Ch\" IsDefault=\"true\" Name=\"SWTAG_MAGIC_NUMBER\" />\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "  <DWord Name=\"1\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"HeapSize\" Format=\"U32\">\n";
    os << "      <Description>Specifies the size in DWORDs of the BXML buffer allocated by the UMD driver.</Description>\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "  <DWord Name=\"2\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"Component\" Format=\"U32\">\n";
    os << "      <Description>Specifies the component type.</Description>\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "</Structure>\n";
}

void SWTagHeapInfo::bxml(std::ostream &os) {
    os << "<Structure Name=\"SWTAG_HEAP_INFO\" Source=\"Driver\" Project=\"All\">\n";
    os << "  <Description>Software tag heap info. This will always be at offset 0.</Description>\n";
    os << "  <DWord Name=\"0\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"MagicNumber\" Format=\"U32\">\n";
    os << "      <Description>This is the target of the MI_STORE_DATA_IMM that specifies where the heap exists. This value will always be 0xDEB06DD1.</Description>\n";
    os << "      <ValidValue Value=\"DEB06DD1h\" IsDefault=\"true\" Name=\"SWTAG_MAGIC_NUMBER\"/>\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "  <DWord Name=\"1\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"HeapSize\" Format=\"U32\">\n";
    os << "      <Description>Specifies the size in DWORDs of the tag heap allocated by the UMD driver. Maximum value is 1MB.</Description>\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "  <DWord Name=\"2\">\n";
    os << "    <BitField HighBit=\"31\" LowBit=\"0\" Name=\"Component\" Format=\"U32\">\n";
    os << "      <Description>Specifies the component type.</Description>\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "</Structure>\n";
}

uint32_t BaseTag::getMarkerNoopID(OpCode opcode) {
    MarkerNoopID id;
    id.OpCode = static_cast<uint32_t>(opcode);
    id.Reserved = 0;
    return id.value;
}

uint32_t BaseTag::getOffsetNoopID(uint32_t offset) {
    OffsetNoopID id;
    id.Offset = offset / sizeof(uint32_t);
    id.SubTag = 0;
    id.MagicBit = 1;
    id.Reserved = 0;
    return id.value;
}

void BaseTag::bxml(std::ostream &os, OpCode opcode, size_t size, const char *name) {
    os << "  <DWord Name=\"0\">\n";
    os << "    <BitField Name=\"DriverDebug\" HighBit=\"31\" LowBit=\"31\" Format=\"OpCode\">\n";
    os << "      <Description>Specifies this is a SW driver debug tag.</Description>\n";
    os << "      <ValidValue Value=\"1\" IsDefault=\"true\" Name=\"DRIVER_DEBUG\" />\n";
    os << "    </BitField>\n";
    os << "    <BitField Name=\"Component\" HighBit=\"30\" LowBit=\"24\" Format=\"OpCode\">\n";
    os << "      <Description>Specifies the component type.</Description>\n";
    os << "      <ValidValue Value=\"1h\" IsDefault=\"true\" Name=\"COMMON\" />\n";
    os << "    </BitField>\n";
    os << "    <BitField Name=\"OpCode\" HighBit=\"23\" LowBit=\"0\" Format=\"OpCode\">\n";
    os << "      <Description>Specifies the opcode.</Description>\n";
    os << "      <ValidValue Value=\"" << static_cast<uint32_t>(opcode) << "\" IsDefault=\"true\" Name=\"" << name << "\" />\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
    os << "  <DWord Name=\"1\">\n";
    os << "    <BitField Name=\"DWORDCount\" HighBit=\"31\" LowBit=\"0\" Format=\"=n\">\n";
    os << "      <Description>Specifies the tag size in DWORDs (TotalSize - 2)</Description>\n";
    os << "      <ValidValue Value=\"" << static_cast<uint32_t>(size / sizeof(uint32_t) - 2) << "\" IsDefault=\"true\" Name=\"DWORD_COUNT_n\" />\n";
    os << "    </BitField>\n";
    os << "  </DWord>\n";
}

void KernelNameTag::bxml(std::ostream &os) {
    os << "<Instruction Name=\"KernelName\" Source=\"Driver\" Project=\"All\" LengthBias=\"2\">\n";
    os << "  <Description>Name of the kernel.</Description>\n";

    BaseTag::bxml(os, OpCode::KernelName, sizeof(KernelNameTag), "KERNEL_NAME");

    unsigned int stringDWORDSize = KENEL_NAME_STR_LENGTH / sizeof(uint32_t);
    os << "  <Dword Name=\"2.." << 2 + stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"KernelName\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Name of the kernel.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";

    os << "</Instruction>\n";
}

void PipeControlReasonTag::bxml(std::ostream &os) {
    os << "<Instruction Name=\"PipeControlReason\" Source=\"Driver\" Project=\"All\" LengthBias=\"2\">\n";
    os << "  <Description>Reason for why the PIPE_CONTROL was inserted.</Description>\n";

    BaseTag::bxml(os, OpCode::PipeControlReason, sizeof(PipeControlReasonTag), "PIPE_CONTROL_REASON");

    unsigned int stringDWORDSize = REASON_STR_LENGTH / sizeof(uint32_t);
    os << "  <Dword Name=\"2.." << 2 + stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"PipeControlReason\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Reason of the PIPE_CONTROL.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";

    os << "</Instruction>\n";
}

void CallNameBeginTag::bxml(std::ostream &os) {
    os << "<Instruction Name=\"CallNameBegin\" Source=\"Driver\" Project=\"All\" LengthBias=\"2\">\n";
    os << "  <Description>ZE Call where the GPU originated from.</Description>\n";

    BaseTag::bxml(os, OpCode::CallNameBegin, sizeof(CallNameBeginTag), "ZE_CALL_NAME_BEGIN");

    unsigned int stringDWORDSize = ZE_CALL_NAME_STR_LENGTH / sizeof(uint32_t);
    os << "  <Dword Name=\"2.." << 2 + stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"CallNameBegin\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Entry of ZE Call where the GPU originated from.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";
    os << "  <Dword Name=\"" << 2 + stringDWORDSize << ".." << 2 + 2 * stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"SWTagId\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Exit of ZE Call where the GPU originated from.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";

    os << "</Instruction>\n";
}

void CallNameEndTag::bxml(std::ostream &os) {
    os << "<Instruction Name=\"CallNameEnd\" Source=\"Driver\" Project=\"All\" LengthBias=\"2\">\n";
    os << "  <Description>ZE Call where the GPU originated from.</Description>\n";

    BaseTag::bxml(os, OpCode::CallNameEnd, sizeof(CallNameEndTag), "ZE_CALL_NAME_END");

    unsigned int stringDWORDSize = ZE_CALL_NAME_STR_LENGTH / sizeof(uint32_t);
    os << "  <Dword Name=\"2.." << 2 + stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"CallNameEnd\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Exit of ZE Call where the GPU originated from.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";
    os << "  <Dword Name=\"" << 2 + stringDWORDSize << ".." << 2 + 2 * stringDWORDSize - 1 << "\">\n";
    os << "    <BitField Name=\"SWTagId\" HighBit=\"" << 32 * stringDWORDSize - 1 << "\" LowBit=\"0\" Format=\"string\">\n";
    os << "      <Description>Exit of ZE Call where the GPU originated from.</Description>\n";
    os << "    </BitField>\n";
    os << "  </Dword>\n";

    os << "</Instruction>\n";
}

SWTagBXML::SWTagBXML() {
    std::ostringstream ss;

    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ss << "<BSpec>\n";

    BXMLHeapInfo::bxml(ss);
    SWTagHeapInfo::bxml(ss);

    KernelNameTag::bxml(ss);
    PipeControlReasonTag::bxml(ss);
    CallNameBeginTag::bxml(ss);
    CallNameEndTag::bxml(ss);

    ss << "</BSpec>";

    str = ss.str();

    if (DebugManager.flags.DumpSWTagsBXML.get()) {
        writeDataToFile("swtagsbxml_dump.xml", str.c_str(), str.size());
    }
}

} // namespace SWTags
} // namespace NEO
