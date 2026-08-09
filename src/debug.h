#ifndef DEBUG_H_
#define DEBUG_H_

#ifdef DEBUG
#define debuglog(...) fprintf(stderr, __VA_ARGS__)
#else
#define debuglog(...)
#endif

#endif
