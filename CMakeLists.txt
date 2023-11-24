cmake_minimum_required(VERSION 3.21.0)
project(bin-embed VERSION 0.1.0 LANGUAGES CXX)

if(MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
    # this change happened in CMake 3.20.4
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks the non stable cpp20 features. For new 16.11 versions
else()
    set(CMAKE_CXX_STANDARD 20) # /std:c++latest for msvc and -std=c++20 for everyone else.
endif()

function(add_resources target)
    set(options PRAGMA HEADER_ONLY)
    set(args NAMESPACE CHUNK HEADER)
    cmake_parse_arguments(PARSE_ARGV 1 EMBED_RES "${options}" "${args}" "")

    if(NOT DEFINED EMBED_RES_HEADER)
        set(EMBED_RES_HEADER "${target}.h")
    endif()

    set(EMBED_GENERATED_DIR ${CMAKE_BINARY_DIR}/embed_${target}/generated)
    set(EMBED_GENERATED_HEADER ${EMBED_GENERATED_DIR}/${EMBED_RES_HEADER})
    set(EMBED_RESOURCE_FILES ${EMBED_RES_UNPARSED_ARGUMENTS})
    list(TRANSFORM EMBED_RESOURCE_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ OUTPUT_VARIABLE EMBED_RESOURCE_FILES_ABS)
    file(RELATIVE_PATH EMBED_GENERATED_HEADER_REL ${CMAKE_BINARY_DIR} ${EMBED_GENERATED_HEADER})

    set(EMBED_ARGS -r ${CMAKE_CURRENT_SOURCE_DIR} -o ${EMBED_GENERATED_HEADER})
    set(EMBED_IMPL_FILES "")

    if(DEFINED EMBED_RES_HEADER_ONLY)
        list(APPEND EMBED_ARGS --header-only)
    else()
        list(TRANSFORM EMBED_RESOURCE_FILES PREPEND ${EMBED_GENERATED_DIR}/ OUTPUT_VARIABLE EMBED_IMPL_FILES)
        list(TRANSFORM EMBED_IMPL_FILES APPEND ".cpp")
    endif()

    if(DEFINED EMBED_RES_PRAGMA)
        list(APPEND EMBED_ARGS --pragma-once)
    endif()

    if(DEFINED EMBED_RES_NAMESPACE)
        list(APPEND EMBED_ARGS --namespace ${EMBED_RES_NAMESPACE})
    endif()

    if(DEFINED EMBED_RES_CHUNK)
        list(APPEND EMBED_ARGS --chunk-size ${EMBED_RES_CHUNK})
    endif()

    list(APPEND EMBED_ARGS ${EMBED_RESOURCE_FILES})

    add_custom_command(OUTPUT "${EMBED_GENERATED_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EMBED_GENERATED_DIR}
        COMMAND bin-embed ${EMBED_ARGS}
        DEPENDS bin-embed ${EMBED_RESOURCE_FILES_ABS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating ${EMBED_GENERATED_HEADER_REL}")

    add_custom_target(EMBED_${target} DEPENDS "${EMBED_GENERATED_HEADER}")

    if(DEFINED EMBED_RES_HEADER_ONLY)
        add_library(${target} INTERFACE)
        target_include_directories(${target} INTERFACE ${EMBED_GENERATED_DIR})
    else()
        add_library(${target} STATIC ${EMBED_IMPL_FILES})
        target_include_directories(${target} PUBLIC ${EMBED_GENERATED_DIR})
    endif()

    add_dependencies(${target} EMBED_${target})
endfunction()

add_executable(bin-embed src/main.cpp src/args.cpp src/writer.cpp)
