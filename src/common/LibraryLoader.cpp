#include "config.h"
#include "LibraryLoader.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace LibraryLoader
{
	handle *OpenLibrary(const char *name)
	{
#ifdef _WIN32
		return reinterpret_cast<handle *>(LoadLibraryA(name));
#else
		return dlopen(name, RTLD_LAZY);
#endif
	}

	void CloseLibrary(handle *handle)
	{
		if (handle)
		{
#ifdef _WIN32
			FreeLibrary(handle);
#else
			dlclose(handle);
#endif
		}
	}

	handle* GetCurrentProcessHandle()
	{
#ifdef _WIN32
		return reinterpret_cast<handle *>(GetModuleHandle(nullptr));
#else
		return RTLD_DEFAULT;
#endif
	}

	function *GetFunction(handle *handle, const char *name)
	{
#ifdef _WIN32
		HMODULE nativeHandle = reinterpret_cast<HMODULE>(handle);
		return reinterpret_cast<function *>(GetProcAddress(nativeHandle, name));
#else
		return reinterpret_cast<function *>(dlsym(handle, name));
#endif
	}
}
