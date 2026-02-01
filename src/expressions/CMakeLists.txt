# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
# Expression engine: QuickJS-based JavaScript evaluator for effect parameters

# --- QuickJS (vendored, MIT license) ---
add_library(quickjs_lib STATIC
    quickjs/quickjs.c
    quickjs/cutils.c
    quickjs/dtoa.c
    quickjs/libregexp.c
    quickjs/libunicode.c
)

target_compile_definitions(quickjs_lib PRIVATE
    CONFIG_VERSION="2025-09-13"
)

# QuickJS requires GNU C extensions (asm volatile, typeof, etc.)
set_target_properties(quickjs_lib PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS ON
)

# Suppress warnings in vendored C code
if(MSVC)
    target_compile_options(quickjs_lib PRIVATE /W0)
else()
    target_compile_options(quickjs_lib PRIVATE -w)
endif()

# PUBLIC so kdenliveLib (which compiles expressionengine.cpp etc.) can find quickjs.h.
# NOTE: The vendored "version" metadata file was removed from quickjs/ to prevent
# shadowing the C++20 <version> standard header.
target_include_directories(quickjs_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/quickjs
)

# On non-Windows, QuickJS needs -lm
if(NOT WIN32)
    target_link_libraries(quickjs_lib PRIVATE m)
endif()

# --- Expression Engine (C++) ---
set(kdenlive_SRCS
    ${kdenlive_SRCS}
    expressions/expressionengine.cpp
    expressions/expressionfunctions.cpp
    expressions/expressioncache.cpp
    expressions/expressiontemplaterepository.cpp
    expressions/expressiontemplatedialog.cpp
    PARENT_SCOPE
)
