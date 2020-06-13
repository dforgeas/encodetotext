#define BTEA_EXPORT
#include "btea.h"

/* golden number phi = (1 + sqrt(5)) / 2
2^32 / phi = 0x9e3779b9 */
#define DELTA 0x9e3779b9
#define MX ((z>>5 ^ y<<2) + (y>>3 ^ z<<4)  ^  (sum^y) + (key[(p&3)^e] ^ z))

BTEA_API
BOOL CALLCONV btea(uint32_t *v, int n, uint32_t const key[4]) {
	uint32_t y, z, sum;
	unsigned p, rounds, e;
	if (n > 1) {          /* Coding Part */
	  rounds = 8 + 69/n; // 33% harder than the stock algorithm
	  sum = 0;
	  z = v[n-1];
	  do {
		sum += DELTA;
		e = sum >> 2 & 3;
		for (p = 0; p < n-1; p++) {
		  y = v[p+1]; 
		  z = v[p] += MX;
		}
		y = v[0];
		z = v[p] += MX; /* p == n-1 */
	  } while (--rounds);
	  return TRUE;
	} else if (n < -1) {  /* Decoding Part */
	  n = -n;
	  rounds = 8 + 69/n;
	  sum = rounds*DELTA;
	  y = v[0];
	  do {
		e = sum >> 2 & 3;
		for (p = n-1; p > 0; p--) {
		  z = v[p-1];
		  y = v[p] -= MX;
		}
		z = v[n-1];
		y = v[0] -= MX;
	  } while ((sum -= DELTA) != 0);
	  return TRUE;
	}
	return FALSE; /* no encoding happened */
}
