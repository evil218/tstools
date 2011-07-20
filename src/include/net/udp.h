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

int udp_open(char *addr, unsigned short port);
int udp_close(int id);
size_t udp_read(int id, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* _UDP_H */
