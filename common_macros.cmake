# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

macro(hide_subdir subdir)
  set(${subdir}_hidden TRUE)
endmacro()

macro(add_subdirectories)
  file(GLOB subdirectories RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
  foreach(subdir ${subdirectories})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${subdir} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt AND NOT ${subdir}_hidden)
      add_subdirectory(${subdir})
    endif()
  endforeach()
endmacro()

macro(create_project_source_tree target)
  if(MSVC)
    set(prefixes ${CMAKE_CURRENT_SOURCE_DIR} ${ARGN})
    get_target_property(source_list ${target} SOURCES)
    foreach(source_file ${source_list})
      if(NOT ${source_file} MATCHES "\<*\>")
        set(source_file_relative ${source_file})
        foreach(prefix ${prefixes})
          if(source_file_relative)
            string(REPLACE "${prefix}" "" source_file_relative ${source_file_relative})
          endif()
        endforeach()
        get_filename_component(source_path_relative ${source_file_relative} PATH)
        if(source_path_relative)
          string(REPLACE "/" "\\" source_path_relative ${source_path_relative})
        endif()
        source_group("Source Files\\${source_path_relative}" FILES ${source_file})
      endif()
    endforeach()
  endif()
endmacro()
