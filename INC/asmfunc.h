// helper for get complex c++ names for use in asm code
#ifdef _PRINT_CPP_NAMES_

#define ASM_FUNCTION {__pragma(message(__FUNCDNAME__" proc\r\n" __FUNCDNAME__ " endp"))}
#define CPP_FUNCTION __pragma(message("extern " __FUNCDNAME__ " : PROC ; "  __FUNCSIG__))

#pragma warning(disable : 4100)
__pragma(message(__FILE__ "(" _CRT_STRINGIZE(__LINE__) "): !! undef _PRINT_CPP_NAMES_ !!"))

#else

#define ASM_FUNCTION
#define CPP_FUNCTION
#endif
