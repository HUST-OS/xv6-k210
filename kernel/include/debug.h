// This header contains some useful macros for debug 

// To avoid namespace conflicts, use `__` before macro name 

#ifndef __DEBUG_H 
#define __DEBUG_H 

// `module_name` must be a C-type string 
#define __INFO(str) 	"[\e[32;1m"str"\e[0m]"
#define __WARN(str) 	"[\e[33;1m"str"\e[0m]"
#define __ERROR(str) 	"[\e[31;1m"str"\e[0m]"

// display message only if it's DEBUG mode 
#ifdef DEBUG 
#include "printf.h"
#define __debug_msg(...) \
	printf(__VA_ARGS__)
#else 
#define __debug_msg(...) \
	do {} while(0)
#endif 

#define __debug_info(mod, ...) \
	__debug_msg(__INFO(mod)": "__VA_ARGS__) 
#define __debug_warn(mod, ...) \
	__debug_msg(__WARN(mod)": "__VA_ARGS__) 
#define __debug_error(mod, ...) \
	__debug_msg(__ERROR(mod)": "__VA_ARGS__)

#endif 
