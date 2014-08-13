
#include "header_helpers.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#ifndef NOSTATS
#include <mm.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include "locking.h"
#include "extras.h"
#include "smsd_cfg.h"
#include "stats.h"
#include "version.h"
#include "blacklist.h"
#include "whitelist.h"
#include "logging.h"
#include "alarm.h"
#include "charset.h"
#include "cfgfile.h"
#include "pdu.h"
#include "modeminit.h"
#include "consts.h"
#include "smsd.h"
#include "system.h"

// Used to select an appropriate header:
char *get_header(const char *header, char *header2)
{
  if (header2 && *header2 && strcmp(header2, "-"))
  {
    if (*header2 == '-')
      return header2 +1;
    return header2;
  }
  return (char *)header;
}

char *get_header_incoming(const char *header, char *header2)
{
  if (!translate_incoming)
    return (char *)header;
  return get_header(header, header2);
}

// Return value: 1/0
// hlen = length of a header which matched.
int test_header(int *hlen, char *line, const char *header, char *header2)
{
  // header2:
  // NULL or "" = no translation
  // "-" = no translation and header is not printed
  // "-Relatório:" = input translated, header is not printed
  // "Relatório:" = input and output translated

  if (header2 && *header2 && strcmp(header2, "-"))
  {
    if (*header2 == '-')
    {
      if (!strncmp(line, header2 +1, *hlen = strlen(header2) -1))
        return 1;
    }
    else if (!strncmp(line, header2, *hlen = strlen(header2)))
      return 1;
  }

  if (!strncmp(line, header, *hlen = strlen(header)))
    return 1;

  *hlen = 0;
  return 0;
}

int prepare_remove_headers(char *remove_headers, size_t size)
{
  char *p;

  if (snprintf(remove_headers, size, "\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\nPDU:",
               HDR_FailReason,
               HDR_Failed,
               HDR_Identity,
               HDR_Modem,
               HDR_Number,
               HDR_Sent,
               HDR_MessageId,
               HDR_Result) >= (ssize_t)size)
    return 0;

  if ((p = get_header(NULL, HDR_FailReason2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Failed2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Identity2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Modem2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Number2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Sent2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_MessageId2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  if ((p = get_header(NULL, HDR_Result2)))
  {
    if (strlen(remove_headers) + strlen(p) +1 < size)
      sprintf(strchr(remove_headers, 0), "\n%s", p);
    else
      return 0;
  }

  return 1;
}
