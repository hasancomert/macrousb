/**
 * net_glue.h -- USB network bring-up (lwIP + DHCP/DNS servers).
 */
#ifndef NET_GLUE_H_
#define NET_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise lwIP, the netif bound to USB, and the DHCP + DNS servers.
 * Call once after tusb_init(). */
void net_init(void);

/* Service lwIP timers; call frequently from the main loop. */
void net_service(void);

/* Device IP as a printable string, e.g. "192.168.7.1". */
const char *net_device_ip_str(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_GLUE_H_ */
