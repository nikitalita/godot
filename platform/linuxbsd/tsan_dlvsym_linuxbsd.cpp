/**************************************************************************/
/*  tsan_dlvsym_linuxbsd.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#if defined(TSAN_ENABLED)
#include <dlfcn.h>

#if defined(__GLIBC__)

// NVIDIA's proprietary Vulkan/GL driver (libnvidia-glcore.so) resolves pthread_* and sem_*
// via dlvsym(libpthread_handle, symbol, "GLIBC_2.2.5"), which bypasses TSAN's normal dlsym/PLT
// interceptors and causes threads spawned during vkCreateDevice to start with a null TSAN Proc*
// allocator state. Route pthread_* and sem_* lookups to TSAN's interceptors in RTLD_DEFAULT.
//
// This file is compiled with -fno-sanitize=thread because TSAN's InitializeCommonInterceptors()
// calls dlvsym(RTLD_NEXT, ...) during _dl_init before TSAN's thread trace state is initialized.
static bool _tsan_has_prefix(const char *p_str, const char *p_prefix) {
	while (*p_prefix) {
		if (*p_str++ != *p_prefix++) {
			return false;
		}
	}
	return true;
}

extern "C" __attribute__((visibility("default"), used)) void *dlvsym(void *p_handle, const char *p_symbol, const char *p_version) {
	using DlvsymFn = void *(*)(void *, const char *, const char *);
	static DlvsymFn real_dlvsym = nullptr;

	const char *volatile sym = p_symbol;
	if (p_handle != RTLD_NEXT && sym != nullptr) {
		if (_tsan_has_prefix(sym, "pthread_") || _tsan_has_prefix(sym, "sem_")) {
			void *intercepted = dlsym(RTLD_DEFAULT, sym);
			if (intercepted != nullptr) {
				return intercepted;
			}
		}
	}
	if (real_dlvsym == nullptr) {
		real_dlvsym = reinterpret_cast<DlvsymFn>(dlsym(RTLD_NEXT, "dlvsym"));
	}
	return real_dlvsym ? real_dlvsym(p_handle, p_symbol, p_version) : dlsym(p_handle, p_symbol);
}
#endif
#endif
