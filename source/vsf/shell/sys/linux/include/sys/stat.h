#ifndef __STAT_H__
#define __STAT_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stat {
    mode_t     st_mode;
};

int stat(const char *pathname, struct stat *buf);

#ifdef __cplusplus
}
#endif

#endif
