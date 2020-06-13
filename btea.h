/* Corrected Block TEA
http://en.wikipedia.org/wiki/XXTEA */
#pragma once
#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef int BOOL;
#define FALSE 0
#define TRUE (!FALSE)

#ifdef _MSC_VER
	#ifdef BTEA_EXPORT
		#define BTEA_API __declspec(dllexport)
	#else
		#define BTEA_API __declspec(dllimport)
	#endif
	#define CALLCONV __stdcall
#else
	#define BTEA_API
	#define CALLCONV
#endif

BTEA_API
BOOL CALLCONV btea(uint32_t *v, int n, uint32_t const key[4]);

#ifdef  __cplusplus
}
#endif

