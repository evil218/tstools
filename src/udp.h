/* vim: set tabstop=8 shiftwidth=8:
 * name: udp.h
 * funx: UDP access
 */

#ifndef _UDP_H
#define _UDP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> /* for uint?_t, etc */

intptr_t udp_open(char *addr, uint16_t port, char *mode);
int udp_close(intptr_t id);
size_t udp_read(intptr_t id, void *buf);
size_t udp_write(intptr_t id, const void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* _UDP_H */
