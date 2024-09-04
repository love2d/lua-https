cmake_minimum_required (VERSION 3.13)

### Basic compilation settings
set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set (CMAKE_CXX_STANDARD 14)

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

### Compiler-specific flags
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options ("-fvisibility=hidden")
	add_link_options ("-fvisibility=hidden")
endif ()

if (LOVR)
  set(LUA_LIBRARIES ${LOVR_LUA})
else()
### Dependencies
	find_package (LuaJIT)
	if (LUAJIT_FOUND)
		set(LUA_INCLUDE_DIR ${LUAJIT_INCLUDE_DIR})
		set(LUA_LIBRARIES ${LUAJIT_LIBRARY})
	else ()
		find_package (Lua 5.1 REQUIRED)
	endif ()

	include_directories (${LUA_INCLUDE_DIR})
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

add_library (https-windows-libraryloader STATIC EXCLUDE_FROM_ALL
	windows/WindowsLibraryLoader.cpp
)

add_library (https-unix-libraryloader STATIC EXCLUDE_FROM_ALL
	generic/UnixLibraryLoader.cpp
)

add_library (https-linktime-libraryloader STATIC EXCLUDE_FROM_ALL
	generic/LinktimeLibraryLoader.cpp
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
	apple/NSURLClient.mm
)

add_library (https-android STATIC EXCLUDE_FROM_ALL
	android/AndroidClient.cpp
)

add_library (https-wininet STATIC EXCLUDE_FROM_ALL
	windows/WinINetClient.cpp
)

### Flags
set (LIBRARY_LOADER_DEFAULT "unix")

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	option (USE_CURL_BACKEND "Use the libcurl backend" ON)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" ON)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (apple-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)
	option (USE_WININET_BACKEND "Use the WinINet backend (windows-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)
elseif (WIN32)
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" ON)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (apple-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)

	if (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		option (USE_WININET_BACKEND "Use the WinINet backend (windows-only)" OFF)
	else ()
		option (USE_WININET_BACKEND "Use the WinINet backend (windows-only)" ON)
	endif ()

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" ON)

	set (LIBRARY_LOADER_DEFAULT "windows")

	# Windows needs to link with Lua libraries
	target_link_libraries(https ${LUA_LIBRARIES})
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (apple-only)" ON)

	# Enabling ARC for apple/NSURLClient.mm
	set_source_files_properties(apple/NSURLClient.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" OFF)
	option (USE_WININET_BACKEND "Use the WinINet backend (windows-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)

	if (NOT IOS)
		# macOS needs -undefined dynamic_lookup
		target_link_options(https PRIVATE -undefined dynamic_lookup)
	else ()
		target_link_libraries(https ${LUA_LIBRARIES})
	endif ()
elseif (ANDROID)
	option (USE_CURL_BACKEND "Use the libcurl backend" OFF)
	option (USE_OPENSSL_BACKEND "Use the openssl backend" OFF)
	option (USE_SCHANNEL_BACKEND "Use the schannel backend (windows-only)" OFF)
	option (USE_NSURL_BACKEND "Use the NSUrl backend (apple-only)" OFF)
	option (USE_ANDROID_BACKEND "Use the Android Java backend (Android-only)" ON)
	option (USE_WININET_BACKEND "Use the WinINet backend (windows-only)" OFF)

	option (USE_WINSOCK "Use winsock instead of BSD sockets (windows-only)" OFF)

	# Android needs to link with Lua libraries
	target_link_libraries(https ${LUA_LIBRARIES})
endif ()
option (DEBUG_SCHANNEL "Enable debug output in schannel backend" OFF)
set (LIBRARY_LOADER ${LIBRARY_LOADER_DEFAULT} CACHE STRING "Which method to use to dynamically load libraries")
set_property (CACHE LIBRARY_LOADER PROPERTY STRINGS "unix;windows;linktime")

set_target_properties(https PROPERTIES PREFIX "")

target_link_libraries (https https-common)

if (USE_CURL_BACKEND)
	set(HTTPS_BACKEND_CURL ON)
	find_package (CURL REQUIRED)
	include_directories (${CURL_INCLUDE_DIRS})
	target_link_libraries (https https-curl)
	target_link_libraries (https-linktime-libraryloader ${CURL_LIBRARY})
endif ()

if (USE_OPENSSL_BACKEND)
	set(HTTPS_BACKEND_OPENSSL ON)
	find_package (OpenSSL REQUIRED)
	include_directories (${OPENSSL_INCLUDE_DIR})
	target_link_libraries (https https-openssl)
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

if (USE_WININET_BACKEND)
	set(HTTPS_BACKEND_WININET ON)
	target_link_libraries (https https-wininet wininet)
endif ()

if (USE_WINSOCK)
	set(HTTPS_USE_WINSOCK ON)
endif ()

if ("${LIBRARY_LOADER}" STREQUAL "unix")
	set(HTTPS_LIBRARY_LOADER_UNIX ON)
	target_link_libraries (https https-unix-libraryloader)
elseif ("${LIBRARY_LOADER}" STREQUAL "windows")
	set(HTTPS_LIBRARY_LOADER_WINDOWS ON)
	target_link_libraries (https https-windows-libraryloader)
elseif ("${LIBRARY_LOADER}" STREQUAL "linktime")
	set(HTTPS_LIBRARY_LOADER_LINKTIME ON)
	target_link_libraries (https https-linktime-libraryloader)
else ()
	message(WARNING "No library loader selected, backends that depend on dynamic loading will be broken")
endif ()

### Generate config-generated.h
add_compile_definitions(HTTPS_HAVE_CONFIG_GENERATED_H)
configure_file (
	common/config-generated.h.in
	common/config-generated.h
)

### Install target
install(TARGETS https DESTINATION .)
