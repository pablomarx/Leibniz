#ifndef __UTILS_H
#define __UTILS_H

#define countof(__a__) (sizeof(__a__) / sizeof(__a__[0]))
#define ROUND_UP(a,b)  (((a) + (b) - 1) & ~((b) - 1))

#endif
