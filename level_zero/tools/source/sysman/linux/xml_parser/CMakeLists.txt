#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(PC_LIBXML_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_XML_PARSER
      ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser_imp.h
      ${CMAKE_CURRENT_SOURCE_DIR}/xml2_api.h
      ${CMAKE_CURRENT_SOURCE_DIR}/xml2_api.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser.h
  )
else()
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_XML_PARSER
      ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser_stub.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser.h
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_XML_PARSER}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_XML_PARSER_LINUX ${L0_SRCS_TOOLS_SYSMAN_XML_PARSER_LINUX})
