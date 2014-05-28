/*----------------------------------------------------------------------------/
 * project: rfc4938
 * file: rfc4938ctl.c
 * version: 1.0
 * date: October 4, 2007
 *
 * Copyright (c) 2013 - Adjacent Link, LLC, Bridgewater NJ
 * Copyright (C), 2007-2008 by cisco Systems, Inc.
 *
 * ===========================
 *
 * This tool is used to read info from the command line the following commands
 * 
 *	show
 *	padq neighbor <neighbor #> max-data-rate <rate> <scalar> cur-data-rate
 * 		<rate> <scalar> latency <milliseconds> resources <percentage>
 *		rel-link-qual <percentage> [receive-only]
 *	padg neighbor <neighbor #> <credits>
 *	initiate { neighbor <neighbor #> | all } <scalar>
 *	terminate { neighbor <neighbor #> | all }
 *
 * ===========================
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *---------------------------------------------------------------------------*/

#include "../rfc4938.h"
#include "../rfc4938_config.h"

int rfc4938ctl_send_show_message(void);

int rfc4938ctl_send_padq_message(
	UINT32_t neighbor_id, 
	UINT16_t max_data_rate, 
	UINT16_t mdr_scale, 
	UINT16_t current_data_rate, 
	UINT16_t cdr_scale, 
	UINT16_t latency, 
	UINT8_t resources, 
	UINT8_t rlq, 
	UINT8_t receive_only);

int rfc4938ctl_send_padg_message(UINT32_t neighbor_id, UINT16_t credits);

int rfc4938ctl_send_initiate_message(int cmd, UINT32_t neighbor_id, UINT16_t credit_scalar);

int rfc4938ctl_send_terminate_message(int cmd, UINT32_t neighbor_id);

int rfc4938ctl_send_message(void *p2buffer, int buflen);

static UINT32_t u32seqnum = 0;

static struct rfc4938config_vars CONFIG;

 /* rfc4938ctl usage information */
static char rfc4938ctl_usage[] =
  "\n"
  "  usage: rfc4938ctl [options]\n"
  "\n"
  "     show\n"
  "     padq neighbor <neighbor #> max-data-rate <rate> <scalar> cur-data-rate\n"
  "           <rate> <scalar> latency <milliseconds> resources <percentage>\n"
  "		rel-link-qual <percentage> [receive-only]\n"
  "     padg neighbor <neighbor #> <credits>\n"
  "     initiate { neighbor <neighbor #> | all } <scalar>\n"
  "     terminate { neighbor <neighbor #> | all }\n" "\n";




static int isNums(char * str)
{
  unsigned int i;

  for(i = 0; i < strlen(str); ++i)
   {
     if(isdigit(str[i]) != 1)
       return 0;
   }

   return 1;
}

/*
 * Show function prints all neighbors
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_send_show_message (void)
{
  void *p2buffer = NULL;
  int buflen;

  p2buffer = malloc (SIZEOF_CLI_SHOW_REQUEST);

  if (p2buffer == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: SIZEOF_CLI_SHOW_REQUEST malloc failed\n");

      return (-1);
    }

  RFC4938_DEBUG_EVENT ("show\n");

  if ((buflen = rfc4938_cli_format_session_show (u32seqnum++, p2buffer)) != SIZEOF_CLI_SHOW_REQUEST)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938_cli_format_session_show failed\n");

      free (p2buffer);

      return (-1);
    }

  if (rfc4938ctl_send_message (p2buffer, buflen) != 0)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938ctl_send_message failed\n");

      free (p2buffer);

      return (-1);
    }

  free (p2buffer);

  return 0;
}

/*
 * rfc4938ctl_send_padq_message function
 *
 * input:
 *     neighbor_id: neighbor number
 *     max_data_rate: max data rate
 *     mdr_scale: max data rate scalar
 *     current_data_rate: current data rate
 *     cdr_scale: current data rate scalar
 *     latency: latency
 *     resources: resources
 *     rlq: relative link quality
 *     receive_only: receive only
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_send_padq_message (UINT32_t neighbor_id,
                              UINT16_t max_data_rate,
                              UINT16_t mdr_scale,
                              UINT16_t current_data_rate,
                              UINT16_t cdr_scale,
                              UINT16_t latency,
                              UINT8_t resources, UINT8_t rlq, UINT8_t receive_only)
{
  void *p2buffer = NULL;
  int buflen;

  p2buffer = malloc (SIZEOF_CLI_PADQ_REQUEST);

  if (p2buffer == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: SIZEOF_CLI_SHOW_REQUEST malloc failed\n");
      return (-1);
    }

  if (receive_only == 0)
    {
      RFC4938_DEBUG_EVENT
        ("rfc4938ctl_send_padq_message neighbor %u, max-data-rate %hu %hu, cur-data-rate"
         " %hu %hu, latency %hu, resources %hu, rel-link-qual %hhu\n", neighbor_id, max_data_rate,
         mdr_scale, current_data_rate, cdr_scale, latency, resources, rlq);
    }
  else if (receive_only == 1)
    {
      RFC4938_DEBUG_EVENT
        ("rfc4938ctl_send_padq_message neighbor %u, max-data-rate %hu %hu, cur-data-rate"
         " %hu %hu, latency %hu, resources %hu, rel-link-qual %hhu, receive-only\n", neighbor_id,
         max_data_rate, mdr_scale, current_data_rate, cdr_scale, latency, resources, rlq);
    }
  else
    {
      RFC4938_DEBUG_ERROR ("ERROR: unknown error in rfc4938ctl_send_padq_message function\n");

      free (p2buffer);

      return (-1);
    }

  if ((buflen = rfc4938_cli_format_padq (u32seqnum++, neighbor_id,
                                         receive_only,
                                         rlq,
                                         resources,
                                         latency,
                                         cdr_scale,
                                         current_data_rate,
                                         mdr_scale,
                                         max_data_rate, p2buffer)) != SIZEOF_CLI_PADQ_REQUEST)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938_cli_format_session_padq failed\n");

      free (p2buffer);

      return (-1);
    }

  if (rfc4938ctl_send_message (p2buffer, buflen) != 0)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938ctl_send_message failed\n");

      free (p2buffer);

      return (-1);
    }

  free (p2buffer);

  return (0);
}

/*
 * rfc4938ctl_send_padg_message
 *
 * input:
 *     neighbor_id: neighbor number
 *     credit: credits
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_send_padg_message (UINT32_t neighbor_id, UINT16_t credits)
{
  void *p2buffer = NULL;
  int buflen;

  p2buffer = malloc (SIZEOF_CLI_PADG_REQUEST);

  if (p2buffer == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: SIZEOF_CLI_SHOW_REQUEST malloc failed\n");

      return (-1);
    }

  RFC4938_DEBUG_EVENT ("rfc4938ctl_send_padg_message neighbor %u credits %hu\n", neighbor_id,
                       credits);
  if ((buflen =
       rfc4938_cli_format_session_padg (u32seqnum++, neighbor_id, credits, p2buffer)) != SIZEOF_CLI_PADG_REQUEST)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938_cli_format_session_padg failed\n");

      free (p2buffer);

      return (-1);
    }

  if (rfc4938ctl_send_message (p2buffer, buflen) != 0)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938ctl_send_message failed\n");

      free (p2buffer);

      return (-1);
    }

  free (p2buffer);

  return 0;
}

/*
 * rfc4938ctl_send_initiate_message
 *
 * input:
 *   cmd: all neighbors or specific
 *   neighbor_id: specific neighbor number
 *   credit_scalar: scalar, 0 specifies no credit scalar tag for PADR
 *
 * return:
 *   SUCCESS
 */
int
rfc4938ctl_send_initiate_message (int cmd, UINT32_t neighbor_id, UINT16_t credit_scalar)
{
  void *p2buffer = NULL;
  int buflen;

  p2buffer = malloc (SIZEOF_CLI_INITIATE_REQUEST);

  if (p2buffer == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: SIZEOF_CLI_SHOW_REQUEST malloc failed\n");
      return (-1);
    }

  if (cmd == 0)
    {
      RFC4938_DEBUG_EVENT ("rfc4938ctl_send_initiate_message all scalar %hu\n", credit_scalar);
      neighbor_id = 0;          /* '0' signifies 'all' neighbors */
    }
  else if (cmd == 1)
    {
      RFC4938_DEBUG_EVENT ("rfc4938ctl_send_initiate_message neighbor %u, scalar %hu\n",
                           neighbor_id, credit_scalar);

    }
  else
    {
      RFC4938_DEBUG_ERROR ("ERROR: unknown error in rfc4938ctl_send_initiate_message function\n");

      free (p2buffer);

      return (-1);
    }
  if ((buflen = rfc4938_cli_format_session_initiate (u32seqnum++, neighbor_id,
                                                     credit_scalar,
                                                     p2buffer)) != SIZEOF_CLI_INITIATE_REQUEST)
    {

      RFC4938_DEBUG_ERROR ("ERROR: rfc4938_cli_format_session_initiate failed\n");

      free (p2buffer);

      return (-1);
    }

  if (rfc4938ctl_send_message (p2buffer, buflen) != 0)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938ctl_send_message failed\n");

      free (p2buffer);

      return (-1);
    }

  free (p2buffer);

  return (0);
}

/*
 * rfc4938ctl_send_terminate_message
 *
 * input:
 *     cmd: all neighbors or specific
 *     neighbor_id: specific neighbor number
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_send_terminate_message (int cmd, UINT32_t neighbor_id)
{
  void *p2buffer = NULL;
  int buflen;
  p2buffer = malloc (SIZEOF_CLI_TERMINATE_REQUEST);

  if (p2buffer == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: SIZEOF_CLI_SHOW_REQUEST malloc failed\n");
      return (-1);
    }

  if (cmd == 0)
    {
      RFC4938_DEBUG_EVENT ("terminate all\n");
      neighbor_id = 0;          /* '0' signifies 'all' neighbors */
    }
  else if (cmd == 1)
    {
      RFC4938_DEBUG_EVENT ("termiante neighbor %u\n", neighbor_id);
    }
  else
    {
      RFC4938_DEBUG_ERROR ("ERROR: unknown error in terminate function\n");

      free (p2buffer);

      return (-1);
    }
  if ((buflen = rfc4938_cli_format_session_terminate (u32seqnum++, neighbor_id,
                                                      p2buffer)) != SIZEOF_CLI_TERMINATE_REQUEST)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938_cli_format_session_terminate failed\n");

      free (p2buffer);

      return (-1);
    }

  if (rfc4938ctl_send_message (p2buffer, buflen) != 0)
    {
      RFC4938_DEBUG_ERROR ("ERROR: rfc4938ctl_send_message failed\n");

      free (p2buffer);

      return (-1);
    }

  free (p2buffer);

  return (0);
}

/*
 * rfc4938ctl_send_message
 *
 * input:
 *     p2buffer: buffer containing data formatted 
 *
 * output:
 *     show: list of neighbors is printed if show is called
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_send_message (void *p2buffer, int buflen)
{
  int z, s;
  int len_inet;
  int my_len_inet;
  socklen_t receive_addr_size;
  struct sockaddr_in adr_srvr;
  struct sockaddr_in my_adr;
  struct sockaddr_in receive_adr;

#define DGRAM_LEN    ( 2048 )
  char dgram[DGRAM_LEN];

  if (p2buffer == NULL)
    {
      return (-1);
    }

  memset (&adr_srvr, 0, sizeof adr_srvr);
  adr_srvr.sin_family = AF_INET;
  adr_srvr.sin_port = htons (CONFIG.client_port);
  adr_srvr.sin_addr.s_addr = inet_addr (LOCALHOST);

  memset (&my_adr, 0, sizeof my_adr);
  my_adr.sin_family = AF_INET;
  my_adr.sin_port = htons (CONFIG.ctl_port);
  my_adr.sin_addr.s_addr = INADDR_ANY;


  if (my_adr.sin_addr.s_addr == INADDR_NONE)
    {
      perror ("error, bad ctll address to bind to");
      return (-1);
    }

  if (adr_srvr.sin_addr.s_addr == INADDR_NONE)
    {
      perror ("error, bad client address to bind to");
      return (-1);
    }

  len_inet = sizeof adr_srvr;
  my_len_inet = sizeof my_adr;

  s = socket (AF_INET, SOCK_DGRAM, 0);

  if (s == -1)
    {
      RFC4938_DEBUG_ERROR ("Unable to create socket\n");
      return (-1);
    }

  z = bind (s, (struct sockaddr *) &my_adr, my_len_inet);

  if (z == -1)
    {
      perror ("error: binding address to socket\n");
      close (s);
      return (-1);
    }

  z = sendto (s, p2buffer, buflen, 0, (struct sockaddr *) &adr_srvr, len_inet);

  if (z < 0)
    {
      perror ("error sending UDP packet");
      close (s);
      return (-1);
    }

  if (((rfc4938_ctl_message_t *) p2buffer)->header.u8cmd_code != CLI_SESSION_SHOW)
    {
      close (s);
      return (0);

    }
  else
    {
      receive_addr_size = sizeof receive_adr;
      z = recvfrom (s, dgram, DGRAM_LEN, 0, (struct sockaddr *) &receive_adr, &receive_addr_size);

      if (z < 0)
        {
          perror ("error receiving UDP packet");
          close (s);
          return (-1);
        }

      printf ("%s\n", dgram);
      close (s);
      return (0);
    }
}

/*
 * Read Config File
 *
 * This function parses the configuration file and assigns
 * the port numbers in the CONFIG struct. 
 *
 * input:
 *     filename: name of config file
 *
 * return:
 *     SUCCESS
 */
int
rfc4938ctl_read_config_file (char *filename)
{
  int i;

  FILE *fp;
#define MAX_INPUT_LENGTH  ( 512 )
  char input_string[MAX_INPUT_LENGTH];

#define ARGC_MAX   ( 5 )
  UINT32_t argc;
  char *argv[ARGC_MAX];

  fp = fopen (filename, "r");
  if (!fp)
    {
      fprintf (stderr, "ERROR: problem opening config file: %s\n", filename);
      return (-1);
    }

  while (fgets (input_string, MAX_INPUT_LENGTH, fp))
    {

      argv[0] = strtok (input_string, " \t\n");
      argc = 1;

      for (i = 1; i < ARGC_MAX; i++)
        {
          argv[i] = strtok (NULL, " \t\n");

          if (argv[i] == NULL)
            {
              break;
            }
          else
            {
              argc++;
            }
        }
      /* empty line */
      if (argv[0] == NULL)
        {
          continue;
        }

      /* max_neighbors */
      else if (strncmp (argv[0], "MAX_NEIGHBORS", strlen ("MAX_NEIGHBORS")) == 0)
        {
          CONFIG.max_nbrs = strtoul (argv[1], NULL, BASE);
        }
      /* port */
      else if (strncmp (argv[0], "PORT", strlen ("PORT")) == 0)
        {
          CONFIG.client_port = strtoul (argv[1], NULL, BASE);
        }
      /* ctl_port */
      else if (strncmp (argv[0], "CTL_PORT", strlen ("CTL_PORT")) == 0)
        {
          CONFIG.ctl_port = strtoul (argv[1], NULL, BASE);
        }
      /* debug_level */
      else if (strncmp (argv[0], "DEBUG_LEVEL", strlen ("DEBUG_LEVEL")) == 0)
        {
          CONFIG.debug_level = strtoul (argv[1], NULL, BASE);
        }
      else
        {
          continue;
        }
    }

  fclose (fp);
  return 0;
}


/*
 * MAIN
 */
int
main (int argc, char *argv[])
{
  rfc4938_debug_set_mask (RFC4938_G_ERROR_DEBUG);

  /* Initialize debugs and ports */
  if (rfc4938ctl_read_config_file (CONFIGPATH) != 0)
    {
      fprintf (stderr, "error reading config file\n");
      return (-1);
    }

  rfc4938_debug_set_mask (RFC4938_G_OFF);

  /* setup debugs */
  if (CONFIG.debug_level >= 1)
    {
      rfc4938_debug_set_mask (RFC4938_G_ERROR_DEBUG);
    }
  if (CONFIG.debug_level >= 2)
    {
      rfc4938_debug_set_mask (RFC4938_G_EVENT_DEBUG);
    }
  if (CONFIG.debug_level >= 3)
    {
      rfc4938_debug_set_mask (RFC4938_G_PACKET_DEBUG);
    }

  if (argv[CMD] == NULL)
    {
      RFC4938_DEBUG_ERROR ("ERROR: Must enter command\n%s\n", rfc4938ctl_usage);
      return (-1);
    }
  else if (!strncmp (argv[CMD], "show", strlen ("show")))
    {                                                         /** show **/
      rfc4938ctl_send_show_message ();
    }
  else if (!strncmp (argv[CMD], "padq", strlen ("padq")))
    {                                                         /** padq **/
      /* check length of command */
      if (argc < RCV || argc > END)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: all arguments" " not included\n");
          return (-1);
        }
      else if (argc == END && strncmp (argv[RCV], "receive-only", strlen ("receive-only")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected: "
                               "'receive-only' read: %s\n", argv[RCV]);
          return (-1);
        }
      /* check words */
      else if (strncmp (argv[PR_STR], "neighbor", strlen ("neighbor")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'neighbor' read: %s\n", argv[PR_STR]);
          return (-1);
        }
      else if (strncmp (argv[MDR_STR], "max-data-rate", strlen ("max-data-rate")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'max-data-rate' read: %s\n", argv[MDR_STR]);
          return (-1);
        }
      else if (strncmp (argv[CDR_STR], "cur-data-rate", strlen ("cur-data-rate")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'cur-data-rate' read: %s\n", argv[CDR_STR]);
          return (-1);
        }
      else if (strncmp (argv[LTNCY_STR], "latency", strlen ("latency")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'latency' read: %s\n", argv[LTNCY_STR]);
          return (-1);
        }
      else if (strncmp (argv[RSRCS_STR], "resources", strlen ("resources")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'resources' read: %s\n", argv[RSRCS_STR]);
          return (-1);
        }
      else if (strncmp (argv[RLQ_STR], "rel-link-qual", strlen ("resources")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected:"
                               " 'rel-link-qual' read: %s\n", argv[RLQ_STR]);
          return (-1);
        }
      /* check numbers */
      else if ((strtoul (argv[PR], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[PR], 0, BASE) > CONFIG.max_nbrs)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid neighbor "
                               "number\nexpected: between 1 and %d, read: %s\n",
                               CONFIG.max_nbrs, argv[PR]);
          return (-1);
        }
      else if ((strtoul (argv[MDR], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[MDR], 0, BASE) > MDR_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid max-data-rate"
                               "\nexpected: between 0 and %d, read: %s\n", MDR_MAX, argv[MDR]);
          return (-1);
        }
      else if ((strtoul (argv[MDR_S], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[MDR_S], 0, BASE) > MDR_SCLR_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid max-data-rate"
                               " scalar\nexpected: between 0 and %d, read: %s\n",
                               MDR_SCLR_MAX, argv[MDR_S]);
          return (-1);
        }
      else if ((strtoul (argv[CDR], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[CDR], 0, BASE) > CDR_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid cur-data-rate"
                               "\nexpected: between 0 and %d, read: %s\n", CDR_MAX, argv[CDR]);
          return (-1);
        }
      else if ((strtoul (argv[CDR_S], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[CDR_S], 0, BASE) > CDR_SCLR_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid cur-data-rate"
                               " scalar\nexpected: between 0 and %d, read: %s\n",
                               CDR_SCLR_MAX, argv[CDR_S]);
          return (-1);
        }
      else if ((strtoul (argv[LTNCY], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[LTNCY], 0, BASE) > LTNCY_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid latency\n"
                               "expected: between 0 and %d, read: %s\n", LTNCY_MAX, argv[LTNCY]);
          return (-1);
        }
      else if ((strtoul (argv[RSRCS], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[RSRCS], 0, BASE) > RSRCS_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid resources\n"
                               "expected: between 0 and %d, read: %s\n", RSRCS_MAX, argv[RSRCS]);
          return (-1);
        }
      else if ((strtoul (argv[RLQ], 0, BASE) == ULONG_MAX && errno == ERANGE) ||
               strtoul (argv[RLQ], 0, BASE) > RLQ_MAX)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid rel-link-qual"
                               "\nexpected: between 0 and %d, read: %s\n", RLQ_MAX, argv[RLQ]);
          return (-1);
        }
      /* execute command: minus receive-only, then with receive only */
      if (argv[16] == NULL)
        {
          rfc4938ctl_send_padq_message (strtoul (argv[PR], 0, BASE), strtoul (argv[MDR], 0, BASE),
                                        strtoul (argv[MDR_S], 0, BASE), strtoul (argv[CDR], 0, BASE),
                                        strtoul (argv[CDR_S], 0, BASE), strtoul (argv[LTNCY], 0, BASE),
                                        strtoul (argv[RSRCS], 0, BASE), strtoul (argv[RLQ], 0, BASE), 0);
          return 0;
        }
      else if (!strncmp (argv[RCV], "receive-only", strlen ("receive-only")))
        {
          rfc4938ctl_send_padq_message (strtoul (argv[PR], 0, BASE), strtoul (argv[MDR], 0, BASE),
                                        strtoul (argv[MDR_S], 0, BASE), strtoul (argv[CDR], 0, BASE),
                                        strtoul (argv[CDR_S], 0, BASE), strtoul (argv[LTNCY], 0, BASE),
                                        strtoul (argv[RSRCS], 0, BASE), strtoul (argv[RLQ], 0, BASE), 1);
          return 0;
        }
      else
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: Unknown error\n%s\n",
                               rfc4938ctl_usage);
          return (-1);
        }
    }
  else if (!strncmp (argv[CMD], "padg", strlen ("padg")))
    {                                                         /** padg **/
      if (argv[PR_STR] == NULL)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadg: no 'neighbor'\n");
          return (-1);
        }
      else if (strncmp (argv[PR_STR], "neighbor", strlen ("neighbor")) != 0)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq expected: 'neighbor'"
                               " read: %s\n", argv[PR_STR]);
          return (-1);
        }
      else if (argv[PR] == NULL)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadg: no <neighbor #>\n");
          return (-1);
        }
      else if (strtoul (argv[PR], 0, BASE) < 1 ||
               strtoul (argv[PR], 0, BASE) > CONFIG.max_nbrs || !isNums (argv[PR]))
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadq: invalid neighbor "
                               "number\nexpected: between 1 and %d, read: %s\n",
                               CONFIG.max_nbrs, argv[PR]);
          return (-1);
        }
      else if (argv[CREDS] == NULL)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadg: no <credits>\n");
          return (-1);
        }
      else if (!(strtoul (argv[CREDS], 0, BASE) == ULONG_MAX && errno == ERANGE) &&
               strtoul (argv[CREDS], 0, BASE) < CREDS_MAX && isNums (argv[CREDS]))
        {
          rfc4938ctl_send_padg_message (strtoul (argv[PR], 0, BASE),
                                        strtoul (argv[CREDS], 0, BASE));
          return 0;
        }
      else
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\npadg: credits must "
                               "be between 0 and %d\n", CREDS_MAX);
          return (-1);
        }
    }
  else if (!strncmp (argv[CMD], "initiate", strlen ("initiate")))
    {                                                                 /** initiate **/
      if (argv[PR_STR] == NULL)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate:" " 'neighbor'\n");
          return (-1);
        }
      else if (!strncmp (argv[PR_STR], "all", strlen ("all")))
        {
          if (argv[SCLR - 1] == NULL)
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate:" " no scalar number\n");
              return (-1);
            }
          else if (!(strtoul (argv[SCLR - 1], 0, BASE) == ULONG_MAX && errno == ERANGE) &&
                   strtoul (argv[SCLR - 1], 0, BASE) < SCLR_MAX)
            {
              rfc4938ctl_send_initiate_message (0, 0, strtoul (argv[SCLR - 1], 0, BASE));
              return 0;
            }
          else
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate:"
                                   " invalid scalar number, must be between "
                                   "0 and %d\n", SCLR_MAX);
              return (-1);
            }
        }
      else if (!strncmp (argv[PR_STR], "neighbor", strlen ("neighbor")))
        {
          if (argv[PR] == NULL)
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate:" " no neighbor number\n");
              return (-1);
            }
          else if (strtoul (argv[PR], 0, BASE) > 0 &&
                   strtoul (argv[PR], 0, BASE) < (unsigned long) (CONFIG.max_nbrs + 1) &&
                   argv[SCLR] == NULL)
            {
              rfc4938ctl_send_initiate_message (1, strtoul (argv[PR], 0, BASE), 0);
              return 0;
            }
          else if (strtoul (argv[PR], 0, BASE) > 0 &&
                   strtoul (argv[PR], 0, BASE) < (unsigned long) (CONFIG.max_nbrs + 1) &&
                   (strtoul (argv[SCLR], 0, BASE) == ULONG_MAX && errno == ERANGE) &&
                   strtoul (argv[SCLR], 0, BASE) < SCLR_MAX)
            {
              rfc4938ctl_send_initiate_message (1, 
                                                strtoul (argv[PR], 0, BASE),
                                                strtoul (argv[SCLR], 0, BASE));
              return 0;
            }
          else
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate:"
                                   " invalid neighbor number or scalar\n");
              return (-1);
            }
        }
      else
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ninitiate: unknown"
                               " error\n%s\n", rfc4938ctl_usage);
        }
    }
  else if (!strncmp (argv[CMD], "terminate", strlen ("terminate")))
    {                                                                   /** terminate **/
      if (argv[PR_STR] == NULL)
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\ntermiante: 'neighbor'\n");
          return (-1);
        }
      else if (!strncmp (argv[PR_STR], "all", strlen ("all")))
        {
          rfc4938ctl_send_terminate_message (0, 0);
          return 0;
        }
      else if (!strncmp (argv[PR_STR], "neighbor", strlen ("neighbor")))
        {
          if (argv[PR] == NULL)
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\nterminate:" " no neighbor number\n");
              return (-1);
            }
          else if (strtoul (argv[PR], 0, BASE) > 0 && strtoul (argv[PR], 0, BASE)
                   < (unsigned long) (CONFIG.max_nbrs + 1))
            {
              rfc4938ctl_send_terminate_message (1, strtoul (argv[PR], 0, BASE));
              return 0;
            }
          else
            {
              RFC4938_DEBUG_ERROR ("ERROR: Invalid Command:\nterminate:"
                                   " unknown error\n%s\n", rfc4938ctl_usage);
              return (-1);
            }
        }
      else
        {
          RFC4938_DEBUG_ERROR ("ERROR: Invalid Command: terminate\n");
          return (-1);
        }
    }
  else if (!strncmp (argv[CMD], "usage", strlen ("usage")))
    {                                                           /** usage **/
      printf ("%s\n", rfc4938ctl_usage);
    }
  else
    {
      RFC4938_DEBUG_ERROR ("ERROR: Invalid Command\n%s\n", rfc4938ctl_usage);
      return (-1);
    }

  return 0;
}

/* EOF */
