cmake_minimum_required (VERSION 3.0)

### Basic compilation settings
set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)
add_definitions (-DNOMINMAX)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

### Compiler-specific flags
if (${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
	set (DLLEXPORT "__declspec(dllexport)")
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	set (DLLEXPORT "__attribute__((visibility(\"default\")))")
	add_compile_options ("-fvisibility=hidden")
	add_link_options ("-fvisibility=hidden")
endif ()

### "Libraries"
add_library (https MODULE
	lua/main.cpp
)

add_library (https-common STATIC
	common/HTTPRequest.cpp
	common/HTTPSClient.cpp
	common/PlaintextConnection.cpp
)

add_library (https-curl STATIC EXCLUDE_FROM_ALL
	generic/CurlClient.cpp
)

add_library (https-openssl STATIC EXCLUDE_FROM_ALL
	generic/OpenSSLConnection.cpp
)

add_library (https-schannel STATIC EXCLUDE_FROM_ALL
	windows/SChannelConnection.cpp
)

add_library (https-nsurl STATIC EXCLUDE_FROM_ALL
	macos/NSURLClient.mm
)

### Flags
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	option (USE_CURL_BACKEND "Use the libcurl backend" ON)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" ON)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" ON)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" ON)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
endif ()

### Dependencies
target_link_libraries (https https-common)

find_package (Lua 5.1 REQUIRED)
include_directories (${LUA_INCLUDE_DIR})
target_link_libraries (https ${LUA_LIBRARIES})

if (USE_CURL_BACKEND)
	find_package (CURL REQUIRED)
	include_directories (${CURL_INCLUDE_DIR})
	target_link_libraries (https https-curl ${CURL_LIBRARIES})
endif ()

if (USE_OPENSSL_BACKEND)
	find_package (OpenSSL REQUIRED)
	include_directories (${OPENSSL_INCLUDE_DIR})
	target_link_libraries (https https-openssl ${OPENSSL_LIBRARIES})
endif ()

if (USE_SCHANNEL_BACKEND)
	target_link_libraries (https https-schannel ws2_32 secur32)
endif ()

if (USE_NSURL_BACKEND)
	target_link_libraries (https https-nsurl)
endif ()

### Generate config.h
configure_file (
	common/config.h.in
	common/config.h
)
