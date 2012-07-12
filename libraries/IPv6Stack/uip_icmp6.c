/**
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *         ICMPv6 echo request and error messages (RFC 4443)
 * \author Julien Abeille <jabeille@cisco.com> 
 * \author Mathilde Durvy <mdurvy@cisco.com>
 */

/*
 * Copyright (c) 2001-2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 */

#include <string.h>
#include "uip_ds6.h"
#include "uip_icmp6.h"

#define DEBUG DEBUG_NONE
#include "uip_debug.h"

#define UIP_IP_BUF                ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF            ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP6_ERROR_BUF  ((struct uip_icmp6_error *)&uip_buf[uip_l2_l3_icmp_hdr_len])

/** \brief temporary IP address */
static uip_ipaddr_t tmp_ipaddr;

/*---------------------------------------------------------------------------*/
void
uip_icmp6_echo_request_input(void)
{
  /*
   * we send an echo reply. It is trivial if there was no extension
   * headers in the request otherwise we need to remove the extension
   * headers and change a few fields
   */
  PRINTF("Received Echo Request from");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("to");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("");
  
  /* IP header */
  UIP_IP_BUF->ttl = uip_ds6_if.cur_hop_limit;

  if(uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)){
    uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &UIP_IP_BUF->srcipaddr);
    uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
  } else {
    uip_ipaddr_copy(&tmp_ipaddr, &UIP_IP_BUF->srcipaddr);
    uip_ipaddr_copy(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
    uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &tmp_ipaddr);
  }

  if(uip_ext_len > 0) {
    /* If there were extension headers*/
    UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
    uip_len -= uip_ext_len;
    UIP_IP_BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
    UIP_IP_BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);
    /* move the echo request payload (starting after the icmp header)
     * to the new location in the reply.
     * The shift is equal to the length of the extension headers present
     * Note: UIP_ICMP_BUF still points to the echo request at this stage
     */
    memmove((uint8_t *)UIP_ICMP_BUF + UIP_ICMPH_LEN - uip_ext_len,
            (uint8_t *)UIP_ICMP_BUF + UIP_ICMPH_LEN, 
            (uip_len - UIP_IPH_LEN - UIP_ICMPH_LEN));
  }
  /* Below is important for the correctness of UIP_ICMP_BUF and the
   * checksum
   */
  uip_ext_len = 0;
  /* Note: now UIP_ICMP_BUF points to the beginning of the echo reply */
  UIP_ICMP_BUF->type = ICMP6_ECHO_REPLY;
  UIP_ICMP_BUF->icode = 0;
  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();
 
  PRINTF("Sending Echo Reply to");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("from");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("");
  UIP_STAT(++uip_stat.icmp.sent);
  return;
}
/*---------------------------------------------------------------------------*/
void
uip_icmp6_error_output(u8_t type, u8_t code, u32_t param) {
  uip_ext_len = 0;

 /* check if originating packet is not an ICMP error*/
  if(UIP_IP_BUF->proto == UIP_PROTO_ICMP6 && UIP_ICMP_BUF->type < 128){
    uip_len = 0;
    return;
  }

  /* remember data of original packet before shifting */
  uip_ipaddr_copy(&tmp_ipaddr, &UIP_IP_BUF->destipaddr);   
    
  uip_len += UIP_IPICMPH_LEN + UIP_ICMP6_ERROR_LEN;
  
  if(uip_len > UIP_LINK_MTU)
    uip_len = UIP_LINK_MTU; 

  memmove((uint8_t *)UIP_ICMP6_ERROR_BUF + UIP_ICMP6_ERROR_LEN,
          (void *)UIP_IP_BUF, uip_len - UIP_IPICMPH_LEN - UIP_ICMP6_ERROR_LEN);

  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = uip_ds6_if.cur_hop_limit;

  /* the source should not be unspecified nor multicast, the check for
     multicast is done in uip_process */
  if(uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)){
    uip_len = 0;
    return;
  }
  
  uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &UIP_IP_BUF->srcipaddr);

  if(uip_is_addr_mcast(&tmp_ipaddr)){
    if(type == ICMP6_PARAM_PROB && code == ICMP6_PARAMPROB_OPTION){
      uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &tmp_ipaddr);
    } else {
      uip_len = 0;
      return;
    }
  } else {
#if UIP_CONF_ROUTER
    /* need to pick a source that corresponds to this node */
    uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &tmp_ipaddr);
#else
    uip_ipaddr_copy(&UIP_IP_BUF->srcipaddr, &tmp_ipaddr);
#endif
  }
  
  UIP_ICMP_BUF->type = type;
  UIP_ICMP_BUF->icode = code;
  UIP_ICMP6_ERROR_BUF->param = uip_htonl(param);
  UIP_IP_BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
  UIP_IP_BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);
  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

  UIP_STAT(++uip_stat.icmp.sent);

  PRINTF("Sending ICMPv6 ERROR message to");
  PRINT6ADDR(&UIP_IP_BUF->destipaddr);
  PRINTF("from");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("");
  return;
}

/*---------------------------------------------------------------------------*/
void
uip_icmp6_send(uip_ipaddr_t *dest, int type, int code, int payload_len)
{

  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->tcflow = 0;
  UIP_IP_BUF->flow = 0;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = uip_ds6_if.cur_hop_limit;
  UIP_IP_BUF->len[0] = (UIP_ICMPH_LEN + payload_len) >> 8;
  UIP_IP_BUF->len[1] = (UIP_ICMPH_LEN + payload_len) & 0xff;

  memcpy(&UIP_IP_BUF->destipaddr, dest, sizeof(*dest));
  uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);

  UIP_ICMP_BUF->type = type;
  UIP_ICMP_BUF->icode = code;

  UIP_ICMP_BUF->icmpchksum = 0;
  UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

  uip_len = UIP_IPH_LEN + UIP_ICMPH_LEN + payload_len;
  tcpip_ipv6_output();
}
/*---------------------------------------------------------------------------*/

/** @} */
