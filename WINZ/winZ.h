#pragma once

#define WINZ_API

#if 0
#ifdef WINZ_STATIC_LIB
#	define WINZ_API
#else
#	ifdef WINZ_EXPORTS
#		define WINZ_API __declspec(dllexport)
#	else
#		define WINZ_API __declspec(dllimport)
#	endif
#endif
#endif
