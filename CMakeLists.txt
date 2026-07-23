cmake_minimum_required(VERSION 3.18.0)
project(h2o-bench-tpcc)

find_library(H2O_LIB h2o-evloop REQUIRED)
find_library(YAJL_LIB yajl REQUIRED)
find_library(Z_LIB z REQUIRED)
find_library(SSL_LIB ssl REQUIRED)
find_library(CRYPTO_LIB crypto REQUIRED)
find_library(FDB_C_LIB fdb_c REQUIRED)

find_path(H2O_INCLUDE h2o.h REQUIRED)
find_path(YAJL_INCLUDE yajl/yajl_gen.h REQUIRED)
find_path(OPENSSL_INCLUDE openssl/ssl.h REQUIRED)
find_path(FDB_INCLUDE foundationdb/fdb_c.h REQUIRED)

include_directories(src ${CMAKE_BINARY_DIR} ${H2O_INCLUDE} ${YAJL_INCLUDE} ${OPENSSL_INCLUDE} ${FDB_INCLUDE})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(H2O_USE_LIBUV=0)

set(COMMON_OPTIONS -flto=auto -pthread -DFDB_API_VERSION=730)
set(WARNING_OPTIONS -pedantic -Wall -Wextra)
add_compile_options(${COMMON_OPTIONS} ${WARNING_OPTIONS})

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_FORTIFY_SOURCE=2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3")

file(GLOB_RECURSE HANDLER_SOURCES "src/handlers/*.c")
file(GLOB SOURCES "src/*.c")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/tpcc_loader.c")

add_executable(${PROJECT_NAME} ${HANDLER_SOURCES} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${COMMON_OPTIONS})
target_link_libraries(${PROJECT_NAME} ${H2O_LIB} ${FDB_C_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${YAJL_LIB} ${Z_LIB} m)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# TPC-C data loader
add_executable(tpcc_loader src/tpcc_loader.c src/tpcc_kv.c)
target_link_libraries(tpcc_loader ${FDB_C_LIB} m)
install(TARGETS tpcc_loader RUNTIME DESTINATION bin)
