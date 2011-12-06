/*
 * vim: set tabstop=8 shiftwidth=8:
 * name: udp.h
 * funx: UDP access
 */

#ifndef _UDP_H
#define _UDP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* for uint?_t, etc */

int udp_open(char *addr, uint16_t port);
int udp_close(int id);
size_t udp_read(int id, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* _UDP_H */
