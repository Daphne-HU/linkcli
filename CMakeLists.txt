cmake_minimum_required(VERSION 3.16)

project(embedded_cli C CXX)

# -----------------------------------------------------------------------
# Library target
#
# When used as a submodule:
#   add_subdirectory(lib/embedded_cli)
#   target_link_libraries(my_app PRIVATE embedded_cli)
#
# That one line adds the include path and compiles cli.c + ring_buffer.c
# into your target — nothing else needed.
# -----------------------------------------------------------------------

add_library(embedded_cli STATIC
    src/cli.c
    src/log.c
    src/ring_buffer.c
)

target_include_directories(embedded_cli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Let the parent project override buffer sizes without touching this repo:
#   target_compile_definitions(embedded_cli PUBLIC CLI_TX_BUF_SIZE=256)
# or via the parent's CMakeLists:
#   add_compile_definitions(CLI_RX_BUF_SIZE=32)

# -----------------------------------------------------------------------
# Host simulator (only built when this is the top-level project)
# -----------------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "embedded_cli: top-level build — including simulator")

    add_executable(cli_sim
        sim/main.c
        sim/uart_stub.c
    )

    target_link_libraries(cli_sim PRIVATE embedded_cli)
endif()
