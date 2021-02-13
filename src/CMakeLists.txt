cmake_minimum_required (VERSION 3.0)

set (HTTPS_SOURCES
	lua/main.cpp
	common/HTTPRequest.cpp
	common/HTTPSClient.cpp
	common/PlaintextConnection.cpp
)
set (HTTPS_LINK_LIBRARIES)
set (HTTPS_INCLUDE_DIRECTORIES
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

### Flags
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	option (USE_CURL_BACKEND "Use the libcurl backend" ON)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" ON)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" ON)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" ON)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
elseif (ANDROID)
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" ON)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
endif ()
option (DEBUG_SCHANNEL "Enable debug output in schannel backend" OFF)

### Dependencies

find_package (Lua 5.1 REQUIRED)
list (APPEND HTTPS_INCLUDE_DIRECTORIES ${LUA_INCLUDE_DIR})
list (APPEND HTTPS_LINK_LIBRARIES ${LUA_LIBRARIES})

if (USE_CURL_BACKEND)
	find_package (CURL REQUIRED)
	list (APPEND HTTPS_SOURCES generic/CurlClient.cpp)
	list (APPEND HTTPS_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIR})
	list (APPEND HTTPS_LINK_LIBRARIES ${CURL_LIBRARIES})
endif ()

if (USE_OPENSSL_BACKEND)
	find_package (OpenSSL REQUIRED)
	list (APPEND HTTPS_SOURCES generic/OpenSSLConnection.cpp)
	list (APPEND HTTPS_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
	list (APPEND HTTPS_LINK_LIBRARIES ${OPENSSL_LIBRARIES})
endif ()

if (USE_SCHANNEL_BACKEND)
	list (APPEND HTTPS_SOURCES windows/SChannelConnection.cpp)
	list (APPEND HTTPS_LINK_LIBRARIES ws2_32 secur32)
endif ()

if (USE_NSURL_BACKEND)
	list (APPEND HTTPS_SOURCES macos/NSURLClient.mm)
endif ()

if (USE_ANDROID_BACKEND)
	list (APPEND HTTPS_SOURCES android/AndroidClient.cpp)
endif ()

### Main library
add_library (https MODULE ${HTTPS_SOURCES})

### Target options
target_include_directories (https PRIVATE ${HTTPS_INCLUDE_DIRECTORIES})
target_compile_definitions (https PRIVATE NOMINMAX)
target_link_libraries (https ${HTTPS_LINK_LIBRARIES})
set_target_properties (https PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	CXX_VISIBILITY_PRESET hidden
	PREFIX ""
)

### Compiler-specific flags
if (MSVC)
	set (DLLEXPORT "__declspec(dllexport)")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set (DLLEXPORT "__attribute__((visibility(\"default\")))")
endif ()

### Generate config.h
configure_file (
	common/config.h.in
	common/config.h
)
