cmake_minimum_required (VERSION 3.13)

### Basic compilation settings
set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

### Compiler-specific flags
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options ("-fvisibility=hidden")
	add_link_options ("-fvisibility=hidden")
endif ()

### "Libraries"
add_library (https MODULE
	lua/main.cpp
)

add_library (https-common STATIC
	common/HTTPS.cpp
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

add_library (https-android STATIC EXCLUDE_FROM_ALL
	android/AndroidClient.cpp
)

### Flags
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	option (USE_CURL_BACKEND "Use the libcurl backend" ON)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" ON)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (macos-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
elseif (WIN32)
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

set_target_properties(https PROPERTIES PREFIX "")

### Dependencies
target_link_libraries (https https-common)

find_package (Lua 5.1 REQUIRED)
include_directories (${LUA_INCLUDE_DIR})
target_link_libraries (https ${LUA_LIBRARIES})

if (USE_CURL_BACKEND)
	set(HTTPS_BACKEND_CURL ON)
	find_package (CURL REQUIRED)
	include_directories (${CURL_INCLUDE_DIR})
	target_link_libraries (https https-curl ${CURL_LIBRARIES})
endif ()

if (USE_OPENSSL_BACKEND)
	set(HTTPS_BACKEND_OPENSSL ON)
	find_package (OpenSSL REQUIRED)
	include_directories (${OPENSSL_INCLUDE_DIR})
	target_link_libraries (https https-openssl ${OPENSSL_LIBRARIES})
endif ()

if (USE_SCHANNEL_BACKEND)
	set(HTTPS_BACKEND_SCHANNEL ON)
	target_link_libraries (https https-schannel ws2_32 secur32)
endif ()

if (USE_NSURL_BACKEND)
	set(HTTPS_BACKEND_NSURL ON)
	set_target_properties(
		https-nsurl
		PROPERTIES
		MACOSX_BUNDLE YES
		XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES  
	)
	target_link_libraries (https "-framework Foundation")
	target_link_libraries (https https-nsurl)
endif ()

if (USE_ANDROID_BACKEND)
	set(HTTPS_BACKEND_ANDROID ON)
	target_link_libraries (https https-android)
	message(STATUS "Ensure to add the Java files to your project too!")
endif ()

### Generate config-generated.h
add_compile_definitions(HTTPS_HAVE_CONFIG_GENERATED_H)
configure_file (
	common/config-generated.h.in
	common/config-generated.h
)
