# lua-https

lua-https is a simple Lua HTTPS module using native platform backends
specifically written for [LÖVE](https://love2d.org) 12.0 and supports
Windows, Linux, macOS, iOS, and Android.

## Reference

To use lua-https, load it with require like `local https = require("https")`.
lua-https does not create global variables!

The https module exposes a single function: `https.request`.

## Synopsis

Simplified form:

```lua
code, body = https.request( url )
```

If you need to specify headers in the request or get them in the
response, you can use the full form:

```lua
code, body, headers = https.request( url, options )
```

### Arguments

* string `url`: HTTP or HTTPS URL to access.
* table `options`: Optional options for advanced mode.
  * string `data`: Additional data to send as application/x-www-form-urlencoded (unless specified otherwise in Content-Type header).
  * string `method`: HTTP method. If absent, it's either "GET" or "POST" depending on the data field above.
  * table `headers`: Additional headers to add to the request as key-value pairs.

### Return values

* number `code`: HTTP status code, or 0 on failure.
* string `body`: HTTP response body or nil on failure.
* table `headers`: HTTP response headers as key-value pairs or nil on failure or option parameter above is nil.

## Compile From Source

While lua-https is bundled in LÖVE 12.0 by default, it's possible to
compile the module from source and use it on earlier version of LÖVE
or Lua. All compilation requires Lua 5.1 (or LuaJIT) headers and libraries.

### Linux

Ensure you have CMake as well as the OpenSSL and cURL development
libraries installed.

```
cmake -Bbuild -S. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PWD/install
cmake --build build --target install
```

`https.so` can be found in the `install` folder.

### Windows

Compilation is done using CMake. This assume MSVC toolchain is
used. Change "x64" to "Win32" to compile for x86 32-bit platform or
"ARM64" for ARM64 platform.

```
cmake -Bbuild -S. -A x64 -DCMAKE_INSTALL_PREFIX=%CD%\install
cmake --build build --config Release --target install
```

https.dll can be found in the install folder.

### Android

Available since LÖVE 11.4
Proper 3rd-party C module support requires this LÖVE version.

Compilation is done by placing lua-https source code in
`<love-android>/love/src/jni/lua-modules`. The structure will look like this:

`<love-android>/love/src/jni/lua-modules/lua-https`
+ example
+ src
+ Android.mk
+ CMakeLists.txt
+ java.txt
+ license.txt

Afterwards compile love-android as usual. The modules will be
automatically embedded to the APK. This can be verified by checking
the APK with Zip viewer application and inspecting files in
lib/arm64-v8a and lib/armeabi-v7a. 

## Copyright

Copyright © 2019-2025 LOVE Development Team

lua-https is licensed under [zLib license](license.txt), same as LÖVE. 
