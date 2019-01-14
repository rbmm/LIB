#pragma once

#ifndef _HAS_CXX17
#ifdef _MSVC_LANG
#if _MSVC_LANG > 201402
#define _HAS_CXX17	1
#else /* _MSVC_LANG > 201402 */
#define _HAS_CXX17	0
#endif /* _MSVC_LANG > 201402 */
#else /* _MSVC_LANG */
#if __cplusplus > 201402
#define _HAS_CXX17	1
#else /* __cplusplus > 201402 */
#define _HAS_CXX17	0
#endif /* __cplusplus > 201402 */
#endif /* _MSVC_LANG */
#endif /* _HAS_CXX17 */

#ifndef _NODISCARD
#if _HAS_CXX17
#define _NODISCARD [[nodiscard]]	
#else
#define _NODISCARD	
#endif
#endif//_NODISCARD
