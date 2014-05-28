/***********************************************************************
*
* pppoe.h
*
* Declaration of various PPPoE constants
*
* Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
* Copyright (C) 2007-2008 Cisco Systems, Inc.
* Copyright (C) 2000 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* $Id: pppoe.h,v 1.31 2006/02/21 00:13:14 dfs Exp $
*
* This file was modified on Feb 2008 by Cisco Systems, Inc.
***********************************************************************/

#ifndef __PPPOE_H__
#define __PPPOE_H__

#include "../pppoe_types.h"
#include "../rfc4938_types.h"
#include "pppoe_rfc4938_debug.h"

extern int IsSetID;

#define _POSIX_SOURCE 1 /* For sigaction defines */

#include <syslog.h>
#include <stdio.h>		/* For FILE */
#include <sys/types.h>		/* For pid_t */
#include <sys/socket.h>
#include <netinet/in.h>


/* But some brain-dead peers disobey the RFC, so frame types are variables */
extern UINT16_t Eth_PPPOE_Discovery;
extern UINT16_t Eth_PPPOE_Session;

extern void switchToRealID(void);
extern void switchToEffectiveID(void);
extern void dropPrivs(void);



/* PPPoE Tags */
#define TAG_END_OF_LIST        0x0000
#define TAG_SERVICE_NAME       0x0101
#define TAG_AC_NAME            0x0102
#define TAG_HOST_UNIQ          0x0103
#define TAG_AC_COOKIE          0x0104
#define TAG_VENDOR_SPECIFIC    0x0105
#define TAG_RELAY_SESSION_ID   0x0110
#define TAG_SERVICE_NAME_ERROR 0x0201
#define TAG_AC_SYSTEM_ERROR    0x0202
#define TAG_GENERIC_ERROR      0x0203

/* Extensions from draft-carrel-info-pppoe-ext-00 */
/* I do NOT like these tags one little bit */
#define TAG_HURL               0x111
#define TAG_MOTM               0x112
#define TAG_IP_ROUTE_ADD       0x121

/* Discovery phase states */
#define STATE_SENT_PADI     0
#define STATE_RECEIVED_PADO 1
#define STATE_SENT_PADR     2
#define STATE_SESSION       3
#define STATE_TERMINATED    4

/* How many PADI/PADS attempts? */
#define MAX_PADI_ATTEMPTS 3
#define MAX_PADR_ATTEMPTS 3

/* Initial timeout for PADO/PADS */
#define PADI_TIMEOUT 5

/* States for scanning PPP frames */
#define STATE_WAITFOR_FRAME_ADDR 0
#define STATE_DROP_PROTO         1
#define STATE_BUILDING_PACKET    2

/* Special PPP frame characters */
#define FRAME_ESC    0x7D
#define FRAME_FLAG   0x7E
#define FRAME_ADDR   0xFF
#define FRAME_CTRL   0x03
#define FRAME_ENC    0x20

#define SMALLBUF   256
#define BIGBUF    2048


/* states for granting credits for rfc4938 */
typedef enum  {
    PADG_SENT = 0,
    PADC_RECEIVED,
} grant_state_t;

/* operating mode with respect to rfc4938 and the credit/metrics scaling */
typedef enum {
      MODE_RFC4938_ONLY    = 0x0,
      MODE_RFC4938_SCALING = 0x1,
} rfc4938_operating_mode;

/* credit scalar requirements in PADS packet */
typedef enum  {
    SCALAR_NEEDED = 0,
    SCALAR_NOT_NEEDED,
    SCALAR_RECEIVED,
} scalar_state_t;




typedef struct PPPOptionStruct {
    unsigned char  opt;	           /* code */
    unsigned char  length;         /* length */
    unsigned char data[0];         /* data */
} __attribute__((packed)) PPPOption;



/* Function passed to parsePacket */
typedef void ParseFunc(UINT16_t type, UINT16_t len, unsigned char *data, void *extra);

/* Keep track of the state of a connection -- collect everything in one spot */

typedef struct PPPoEConnectionStruct {
    int discoveryState;		     /* Where we are in discovery */
    int udpIPCSocket;                /* udp socket for client ipc */
    int signalPipe[2];               /* pipe event signals */
    unsigned char myEth[PPPOE_ETH_ALEN];   /* My MAC address */
    unsigned char peerEth[PPPOE_ETH_ALEN]; /* Peer's MAC address */
    UINT16_t sessionId;		     /* Session ID */
    char *ifName;		     /* Interface name */
    char *serviceName;		     /* Desired service name, if any */
    char *acName;		     /* Desired AC name, if any */
    int useHostUniq;		     /* Use Host-Uniq tag */
    int numPADOs;		     /* Number of PADO packets received */
    PPPoETag cookie;		     /* We have to send this if we get it */
    PPPoETag relayId;		     /* Ditto */
    int PADSHadError;                /* If PADS had an error tag */
    int discoveryTimeout;            /* Timeout for discovery packets */

/* rfc4938 and credit/metric scaling */

    UINT16_t local_credits;           /* local credits per rfc4938 */
    UINT16_t local_credit_scalar;     /* scalar for these credits */
    UINT16_t peer_credits;            /* peer credits per rfc4938 */
    UINT16_t peer_credit_scalar;      /* scalar for these credits */
    UINT16_t credit_cache;            /* saved peer credits  */
    UINT16_t credits_pending_padc;    /* credits pending padc  */
    UINT16_t grant_limit;             /* max amount we are going to grant in PADG */
    UINT16_t my_port;                 /* port to listen to pkts on */
    UINT32_t peer_pid;                /* peer pid */
    UINT16_t parent_port;             /* port for rfc4938 proc */
    struct sockaddr_in my_saddr;      /* my sock_addr */
    UINT32_t peer_id;                 /* peer id */
    UINT32_t parent_id;               /* my parent id */
    UINT8_t  send_inband_grant;       /* flag to set to send an inband grant */
    UINT32_t padg_tries;              /* number of times a padg has been sent */
    time_t   padg_initial_send_time;  /* time we sent first padg */
    time_t   padg_retry_send_time;    /* time we sent a retry padg */
    grant_state_t grant_state;        /* state of padg sent */
    scalar_state_t scalar_state;      /* if scalar tag is needed */
    rfc4938_operating_mode mode;      /* operating mode for 4938 and scaling */
    int timed_credits;                /* enable credits by timer */
    int p2p_mode;                     /* p2p or broadcast mode */
    int lcp_mode;                     /* lcp mode */
    UINT32_t host_id;                 /* the host id */
    UINT32_t local_magic;             /* my magic id */
    UINT32_t peer_magic;              /* peer magic id */
    int enable_lcp_echo_reply;        /* enable lcp echo reply */
    
} PPPoEConnection;

/* Structure used to determine acceptable PADO or PADS packet */
struct PacketCriteria {
    PPPoEConnection *conn;
    int acNameOK;
    int serviceNameOK;
    int seenACName;
    int seenServiceName;
};

/* Function Prototypes */
UINT16_t getEtherType(PPPoEPacket *packet);



int handle_session_frame_from_ac (PPPoEConnection * conn, PPPoEPacket * packet, int len);

int consume_credits_and_send_frame_to_ac (PPPoEConnection * conn, PPPoEPacket * packet);

int send_session_packet_to_ac(PPPoEConnection *conn, PPPoEPacket *pkt);

int send_discovery_packet_to_ac(PPPoEConnection *conn, PPPoEPacket *pkt);

void handle_credit_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn);

void sync_credit_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn);

void handle_inband_grant(PPPoEConnection *conn, UINT16_t fcn, UINT16_t bcn);

void fatalSys(char const *str, char const *err);

void rp_fatal(char const *str);

void printErr(char const *str);

void sysErr(char const *str);

#ifdef DEBUGGING_ENABLED
void dumpPacket(FILE *fp, PPPoEPacket *packet, char const *dir);

void printPacket(PPPoEPacket *packet, char const *dir);

void dumpHex(FILE *fp, unsigned char const *buf, int len);

void printHex(unsigned char const *buf, int len);
#endif

int parseDiscoveryPacket(PPPoEPacket *packet, ParseFunc *func, void *extra);

void parseLogErrs(UINT16_t typ, UINT16_t len, unsigned char *data, void *xtra);

void pktLogErrs(char const *pkt, UINT16_t typ, UINT16_t len, unsigned char *data, void *xtra);


char *strDup(char const *str);

void sendPADTandExit(PPPoEConnection *conn, char const *msg, int tellParent);

void sendPADTf(PPPoEConnection *conn, char const *fmt, ...);

void sendSessionPacket(PPPoEConnection *conn, PPPoEPacket *packet, int len);

void doDiscovery(PPPoEConnection *conn);

unsigned char *findTag(PPPoEPacket *packet, UINT16_t tagType, PPPoETag *tag);

PPPoEConnection *get_pppoe_conn(void);

#define SET_STRING(var, val) do { if (var) free(var); var = strDup(val); } while(0);

#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
        syslog(LOG_ERR, "Would create too-long packet"); \
        return; \
    } \
} while(0)

/* True if Ethernet address is broadcast or multicast */
#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) == 0xFF)
#define NOT_BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) != 0xFF)

#define SWAP_PACKET_DST_SRC(packet) do { UINT8_t tmp[6]; \
                                         memcpy (tmp, packet->eth_hdr.dest, PPPOE_ETH_ALEN); \
                                         memcpy (packet->eth_hdr.dest, packet->eth_hdr.source, PPPOE_ETH_ALEN); \
                                         memcpy (packet->eth_hdr.source, tmp, PPPOE_ETH_ALEN); } while(0)

#define BUMP_CREDITS(x, y, z) do { if(((x) + (*y)) > MAX_CREDITS) { \
                                      (*y) = MAX_CREDITS;           \
                                    }                               \
                                   else {                           \
                                     (*y) += (x);                   \
                                    }                               \
                                   if((*y) > z) {                   \
                                     (*y) = z;                      \
                                   }                                \
                                } while(0);
 
#endif
