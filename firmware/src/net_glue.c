/**
 * net_glue.c -- bind lwIP to TinyUSB's USB network class, and run the
 * on-device DHCP server (hands the host 192.168.7.2) and a small DNS server.
 *
 * Network layout:
 *   device : 192.168.7.1   (this MCU, serves the web UI)
 *   host   : 192.168.7.2   (your PC, via DHCP)
 *
 * The lwIP netif / tud_network_* plumbing follows TinyUSB's net_lwip_webserver
 * example (MIT, (c) 2020 Peter Lawrence).
 */
#include "net_glue.h"

#include "tusb.h"
#include "class/net/net_device.h"

#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

#include "dhserver.h"
#include "dnserver.h"

#include <string.h>

#define INIT_IP4(a, b, c, d)  { PP_HTONL(LWIP_MAKEU32(a, b, c, d)) }

/* Device-side MAC (also referenced by usb_descriptors.c). First byte 0x02 =
 * locally-administered. */
uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

static struct netif s_netif;

static const ip4_addr_t s_ipaddr  = INIT_IP4(192, 168, 7, 1);
static const ip4_addr_t s_netmask = INIT_IP4(255, 255, 255, 0);
static const ip4_addr_t s_gateway = INIT_IP4(0, 0, 0, 0);

/* Addresses the DHCP server may hand out (kept in RAM: stores client MACs). */
static dhcp_entry_t s_dhcp_entries[] = {
  { {0}, INIT_IP4(192, 168, 7, 2), 24 * 60 * 60 },
  { {0}, INIT_IP4(192, 168, 7, 3), 24 * 60 * 60 },
  { {0}, INIT_IP4(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t s_dhcp_config = {
  .router = INIT_IP4(0, 0, 0, 0),
  .port   = 67,
  .dns    = INIT_IP4(192, 168, 7, 1),
  .domain = "usb",
  .num_entry = TU_ARRAY_SIZE(s_dhcp_entries),
  .entries   = s_dhcp_entries,
};

const char *net_device_ip_str(void)
{
  return "192.168.7.1";
}

//--------------------------------------------------------------------+
// lwIP netif callbacks
//--------------------------------------------------------------------+
static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
  (void)netif;
  for (;;) {
    if (!tud_ready()) return ERR_USE;
    if (tud_network_can_xmit(p->tot_len)) {
      tud_network_xmit(p, 0);
      return ERR_OK;
    }
    tud_task();  /* let TinyUSB flush the previous frame */
  }
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr)
{
  return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif)
{
  netif->mtu   = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL;
  netif->name[0] = 'u';
  netif->name[1] = 's';
  netif->linkoutput = linkoutput_fn;
  netif->output     = ip4_output_fn;
  return ERR_OK;
}

static void link_state_cb(struct netif *netif)
{
  tud_network_link_state(BOARD_TUD_RHPORT, netif_is_link_up(netif));
}

//--------------------------------------------------------------------+
// TinyUSB network class callbacks
//--------------------------------------------------------------------+
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  if (size) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p == NULL) return false;   /* retry later */
    pbuf_take(p, src, size);
    if (s_netif.input(p, &s_netif) != ERR_OK) {
      pbuf_free(p);
    }
    tud_network_recv_renew();
  }
  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  (void)arg;
  struct pbuf *p = (struct pbuf *)ref;
  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

void tud_network_init_cb(void)
{
  /* nothing to re-init on reconnect for this app */
}

//--------------------------------------------------------------------+
// DNS server: resolve our hostname(s) to the device IP
//--------------------------------------------------------------------+
static bool dns_query_proc(const char *name, ip4_addr_t *addr)
{
  if (0 == strcmp(name, "macrousb") || 0 == strcmp(name, "macrousb.usb") ||
      0 == strcmp(name, "setup.usb")) {
    *addr = s_ipaddr;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+
void net_init(void)
{
  lwip_init();

  struct netif *netif = &s_netif;
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;  /* device MAC must differ from host MAC */

  netif = netif_add(netif, &s_ipaddr, &s_netmask, &s_gateway, NULL, netif_init_cb, ethernet_input);
  netif_set_default(netif);

  netif_set_link_callback(netif, link_state_cb);
  netif_set_link_up(netif);

  while (dhserv_init(&s_dhcp_config) != ERR_OK) {}
  while (dnserv_init(IP_ADDR_ANY, 53, dns_query_proc) != ERR_OK) {}
}

void net_service(void)
{
  sys_check_timeouts();
}

//--------------------------------------------------------------------+
// lwIP NO_SYS support hooks
//--------------------------------------------------------------------+
sys_prot_t sys_arch_protect(void)          { return 0; }
void       sys_arch_unprotect(sys_prot_t p){ (void)p; }

uint32_t sys_now(void)
{
  return tusb_time_millis_api();
}
