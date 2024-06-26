# MIT License
#
# Copyright (c) 2015-2021 The ViaDuck Project
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#


cmake_minimum_required(VERSION 3.2)
project(openssl-cmake)

if (WIN32)
    set(BUILD_OPENSSL ON CACHE BOOL "Automated OpenSSL building")
    set(OPENSSL_BUILD_VERSION "1.1.1f" CACHE STRING "OpenSSL version to build")
    set(OPENSSL_INSTALL_MAN OFF CACHE STRING "Install man pages?")
    set(OPENSSL_MODULES "no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 no-engine no-idea no-mdc2 no-rc5 no-camellia no-ssl3 no-heartbeats no-gost no-deprecated no-capieng no-comp no-dtls no-psk no-srp no-dso no-dsa no-rc2 no-des" CACHE STRING "OpenSSL configure options")
    set(CROSS_ANDROID OFF CACHE BOOL "Cross-compiling for Android?")
    set(CROSS ON CACHE BOOL "Cross-compiling?")
    set(CROSS_TARGET "mingw64" CACHE STRING "Cross-compilation target")
    set(CROSS_PREFIX "x86_64-w64-mingw32-")
    set(SYSTEM_OPENSSL OFF CACHE STRING "Use system-provided openssl libraries (instead of prebuilts or building)")
else()
    set(BUILD_OPENSSL ON CACHE BOOL "Automated OpenSSL building")
    set(OPENSSL_BUILD_VERSION "1.1.1f" CACHE STRING "OpenSSL version to build")
    set(OPENSSL_INSTALL_MAN OFF CACHE STRING "Install man pages?")
    set(OPENSSL_MODULES "no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 no-engine no-idea no-mdc2 no-rc5 no-camellia no-ssl3 no-heartbeats no-gost no-deprecated no-capieng no-comp no-dtls no-psk no-srp no-dso no-dsa no-rc2 no-des" CACHE STRING "OpenSSL configure options")
    set(CROSS_ANDROID OFF CACHE BOOL "Cross-compiling for Android?")
    set(CROSS OFF CACHE BOOL "Cross-compiling?")
    set(CROSS_TARGET "mingw64" CACHE STRING "Cross-compilation target")
    set(CROSS_PREFIX "x86_64-w64-mingw32-")
    set(SYSTEM_OPENSSL OFF CACHE STRING "Use system-provided openssl libraries (instead of prebuilts or building)")
endif()

# allow including our modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# mimic system ssl and crypto targets
add_library(ssl INTERFACE)
add_library(crypto INTERFACE)

if (SYSTEM_OPENSSL)
    # use system provided openssl
    find_package(OpenSSL REQUIRED)

    # link fake targets
    target_link_libraries(ssl INTERFACE OpenSSL::SSL)
    target_link_libraries(crypto INTERFACE OpenSSL::Crypto)
    add_custom_target(openssl)
else()
    # build our own or use prebuilts
    
    # set up fake targets
    add_library(ssl_lib STATIC IMPORTED GLOBAL)
    add_library(crypto_lib STATIC IMPORTED GLOBAL)

    target_link_libraries(ssl INTERFACE ssl_lib)
    target_link_libraries(crypto INTERFACE crypto_lib)

    # set up prefix
    if (BUILD_OPENSSL)
        set(OPENSSL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
    else()
        set(OPENSSL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openssl-prefix/src/openssl)
    endif()

    if (CROSS)
        set(OPENSSL_LIBSSL_PATH ${OPENSSL_PREFIX}/usr/local/lib/libssl.dll.a)
        set(OPENSSL_LIBCRYPTO_PATH ${OPENSSL_PREFIX}/usr/local/lib/libcrypto.dll.a)
    else()
        set(OPENSSL_LIBSSL_PATH ${OPENSSL_PREFIX}/usr/local/lib/libssl.a)
        set(OPENSSL_LIBCRYPTO_PATH ${OPENSSL_PREFIX}/usr/local/lib/libcrypto.a)
    endif()

    # set up openssl target
    if (BUILD_OPENSSL)
        include(BuildOpenSSL)
    else()
        include(PrebuiltOpenSSL)
    endif()
    

    # set import locations
    set_target_properties(ssl_lib PROPERTIES IMPORTED_LOCATION ${OPENSSL_LIBSSL_PATH})
    set_target_properties(crypto_lib PROPERTIES IMPORTED_LOCATION ${OPENSSL_LIBCRYPTO_PATH})


    # set include locations
    target_include_directories(ssl BEFORE INTERFACE $<BUILD_INTERFACE:${OPENSSL_PREFIX}/usr/local/include>)
    target_include_directories(crypto BEFORE INTERFACE $<BUILD_INTERFACE:${OPENSSL_PREFIX}/usr/local/include>)

    # add fake targets to common target
    add_dependencies(ssl_lib openssl)
    add_dependencies(crypto_lib openssl)
endif()
