cmake_minimum_required(VERSION 3.28.3)
project(gckb C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)

add_executable(gckb src/gckb.c)

target_include_directories(gckb PRIVATE "include" ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})

target_include_directories(gckb PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

target_link_libraries(gckb PRIVATE ${GLIB_LIBRARIES} ${GIO_LIBRARIES})

target_compile_definitions(gckb PRIVATE GLIB GIO)
