cmake_minimum_required(VERSION 3.0)
include(CheckCCompilerFlag)
project(oka)

subdirs(src)

if(POLICY CMP0065)
    cmake_policy(SET CMP0065 NEW)
endif()

macro(check_add_c_flag flag)
    string(REPLACE "=" "_" CHECK_ADD_FLAG_NAME "have${flag}")
    check_c_compiler_flag(${flag} ${CHECK_ADD_FLAG_NAME})
    if(${${CHECK_ADD_FLAG_NAME}})
        list(APPEND C_FLAGS ${flag})
    endif()
endmacro()

macro(check_add_ld_flag flag)
    string(REPLACE "=" "_" CHECK_ADD_FLAG_NAME "have${flag}")
    set(CMAKE_REQUIRED_FLAGS ${flag})
    check_c_compiler_flag("" ${CHECK_ADD_FLAG_NAME})
    if(${${CHECK_ADD_FLAG_NAME}})
        list(APPEND LD_FLAGS ${flag})
    endif()
    set(CMAKE_REQUIRED_FLAGS "")
endmacro()

check_add_c_flag(-Weverything)
check_add_c_flag(-Wall)
check_add_c_flag(-Wextra)
check_add_c_flag(-Werror=incompatible-pointer-types)
check_add_c_flag(-Wno-disabled-macro-expansion)
check_add_c_flag(-Wno-used-but-marked-unused)
check_add_c_flag(-Wno-gnu-auto-type)
check_add_c_flag(-Wno-c++98-compat)
check_add_c_flag(-Wno-keyword-macro)
check_add_c_flag(-Wno-padded)
check_add_c_flag(-Wno-packed)
check_add_c_flag(-Wno-missing-braces)
check_add_c_flag(-Wno-missing-field-initializers)
check_add_c_flag(-fdata-sections)
check_add_c_flag(-ffunction-sections)

check_add_ld_flag(-Wl,--gc-sections)

list(APPEND C_FLAGS
    -std=gnu11 -pthread -g
)

string(REPLACE ";" " " C_FLAGS_STR "${C_FLAGS}")
string(REPLACE ";" " " LD_FLAGS_STR "${LD_FLAGS}")

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_MODULE_PREFIX "")
set(CMAKE_SHARED_MODULE_SUFFIX ".so")
set(CMAKE_EXE_LINKER_FLAGS ${LD_FLAGS_STR})
set(CMAKE_MODULE_LINKER_FLAGS ${LD_FLAGS_STR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_STR}")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/oka")

set(OKA_DIR "${CMAKE_INSTALL_PREFIX}/lib/oka")
configure_file(oka.in oka @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/oka" DESTINATION bin)

set(OKA_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
configure_file(oka.in "${CMAKE_CURRENT_SOURCE_DIR}/oka" @ONLY)

include_directories(include)
