cmake_minimum_required(VERSION 3.16)

# C is needed in addition to C++ because libcurl, libssh and mbedTLS are C libraries.
project(ftp_sftp_exercise LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Without an explicit build type, g++ compiles without debug symbols and
# breakpoints in CLion silently do not work.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Lets CLion resolve the headers, so curl.h and libssh.h are not underlined in red.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep the binaries directly in the build directory for every generator
# (otherwise they land in build/Debug/).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}")
endforeach()

# ---------------------------------------------------------------------------
# Dependencies: libcurl (FTP) and libssh (SFTP)
#
# Windows: nothing needs to be installed. CMake downloads both libraries on the
# first configure run and builds them along with the project. That takes a few
# minutes ONCE - afterwards everything is cached in the build directory.
# In CLion it is enough to open the project and wait for the CMake reload.
#
# Linux/macOS: the system packages are used, because every system already
# provides them:
#     sudo apt install libcurl4-openssl-dev libssh-dev
# ---------------------------------------------------------------------------
if(WIN32)
    include(FetchContent)

    # The libraries are only built, never "installed". This also prevents
    # libssh from trying to export the mbedTLS targets along with it.
    set(CMAKE_SKIP_INSTALL_RULES ON)

    # Build everything statically: that produces a single self-contained .exe
    # without any DLLs that would have to sit next to it.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING     OFF CACHE BOOL "" FORCE)

    message(STATUS "Windows: downloading libcurl and libssh (only on the first run) ...")

    # --- mbedTLS ----------------------------------------------------------
    # libssh needs a cryptographic backend. OpenSSL is not buildable with CMake
    # alone (it needs Perl), mbedTLS is - therefore mbedTLS is used here.
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(mbedtls
        URL      https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2
        URL_HASH SHA256=8b54fb9bcf4d5a7078028e0520acddefb7900b3e66fec7f7175bb5b7d85ccdca)
    FetchContent_MakeAvailable(mbedtls)

    # mbedTLS must be built WITH thread support, otherwise libssh's
    # crypto_thread_init() returns SSH_ERROR (src/threads/mbedtls.c). libssh
    # then aborts its initialisation before ssh_socket_init(), prints
    # "Error in auto_init()" and crashes later on an uninitialised poll
    # function pointer. PUBLIC, so that libssh sees the same configuration.
    foreach(mbedtls_target IN ITEMS mbedcrypto mbedx509 mbedtls)
        target_compile_definitions(${mbedtls_target}
                PUBLIC MBEDTLS_THREADING_C MBEDTLS_THREADING_PTHREAD)
    endforeach()

    # --- libssh (SFTP) ----------------------------------------------------
    # libssh looks for mbedTLS with find_library/find_path. Those write into
    # cache variables and are skipped if the variable is already set - so we
    # point them at the mbedTLS we just built.
    set(MBEDTLS_INCLUDE_DIR   "${mbedtls_SOURCE_DIR}/include" CACHE PATH   "" FORCE)
    set(MBEDTLS_SSL_LIBRARY    mbedtls                        CACHE STRING "" FORCE)
    set(MBEDTLS_CRYPTO_LIBRARY mbedcrypto                     CACHE STRING "" FORCE)
    set(MBEDTLS_X509_LIBRARY   mbedx509                       CACHE STRING "" FORCE)

    set(WITH_MBEDTLS  ON  CACHE BOOL "" FORCE)
    set(WITH_SERVER   OFF CACHE BOOL "" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WITH_ZLIB     OFF CACHE BOOL "" FORCE)
    set(WITH_GSSAPI   OFF CACHE BOOL "" FORCE)
    set(UNIT_TESTING  OFF CACHE BOOL "" FORCE)
    set(CLIENT_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(libssh
        URL      https://github.com/libssh/libssh-mirror/archive/refs/tags/libssh-0.11.1.tar.gz
        URL_HASH SHA256=b43ef9c91b6c3db64e7ba3db101eb89dbe645db63489c19d4f88cf6f84911ec6)
    FetchContent_MakeAvailable(libssh)

    # --- libcurl (FTP) ----------------------------------------------------
    # Schannel is the TLS implementation built into Windows - that way libcurl
    # does not need OpenSSL either.
    set(CURL_USE_SCHANNEL ON  CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE    OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FTP  OFF CACHE BOOL "" FORCE)   # we need FTP!
    set(CURL_USE_LIBPSL   OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2  OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH   OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB         OFF CACHE STRING "" FORCE)
    set(CURL_BROTLI       OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD         OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(curl
        URL      https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.gz
        URL_HASH SHA256=a889ac9dbba3644271bd9d1302b5c22a088893719b72be3487bc3d401e5c4e80)
    FetchContent_MakeAvailable(curl)

    set(LIBSSH_TARGET ssh)
else()
    find_package(CURL QUIET)
    if(NOT CURL_FOUND)
        message(FATAL_ERROR
            "libcurl was not found.\n"
            "Install it with: sudo apt install libcurl4-openssl-dev libssh-dev")
    endif()

    # The CMake config exports the target as "ssh::ssh", older versions only
    # know "ssh". If there is no config, pkg-config is used.
    find_package(libssh CONFIG QUIET)
    if(TARGET ssh::ssh)
        set(LIBSSH_TARGET ssh::ssh)
    elseif(TARGET ssh)
        set(LIBSSH_TARGET ssh)
    else()
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(LIBSSH QUIET IMPORTED_TARGET libssh)
        endif()
        if(TARGET PkgConfig::LIBSSH)
            set(LIBSSH_TARGET PkgConfig::LIBSSH)
        else()
            message(FATAL_ERROR
                "libssh was not found.\n"
                "Install it with: sudo apt install libssh-dev")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Two programs are built from this project:
#
#   without_interface  - the finished starting point. Builds and runs right
#                        away; read and run this one first.
#
#   with_interface     - YOUR TASK. This does NOT compile at the beginning,
#                        because main() uses the interfaces you still have to
#                        write. The compiler errors are your to-do list.
#
# In CLion you choose between the two in the run configuration drop-down at
# the top right.
# ---------------------------------------------------------------------------
add_executable(without_interface src/without_interface.cpp)
add_executable(with_interface    src/with_interface.cpp)

# Both programs need the same libraries and the same settings, so they are
# configured together in one loop.
foreach(exercise_target IN ITEMS without_interface with_interface)
    target_link_libraries(${exercise_target} PRIVATE CURL::libcurl ${LIBSSH_TARGET})

    if(WIN32)
        # Tells the curl headers that we link the static library.
        target_compile_definitions(${exercise_target} PRIVATE CURL_STATICLIB)

        # Also link the compiler's own runtime libraries into the executable.
        # Without this the .exe only starts from within CLion and otherwise
        # fails with "libstdc++-6.dll not found".
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(${exercise_target} PRIVATE -static)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${exercise_target} PRIVATE -Wall -Wextra)
    endif()
endforeach()

message(STATUS "libssh is linked via: ${LIBSSH_TARGET}")
