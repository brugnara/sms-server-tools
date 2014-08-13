
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


/* =======================================================================
   Remove and add headers in a message file
   - remove_headers: "\nHeader1:\nHeader2:" (\n and : are delimiters)
   - add_buffer is an additional header data wich is added after add_headers.
     This data is not processed with format string. For example to be used
     with pdu store which can be very large and there is no reason to alloc
     memory for just passing this data.
   - add_headers: "Header1: value\nHeader2: value\n" (actual line(s) to write)
   ======================================================================= */

int change_headers(char *filename, char *remove_headers, char *add_buffer, char *add_headers, ...)
{
  int result = 0;
  char line[1024];
  char header[1024 +1];
  char *p;
  int in_headers = 1;
  char tmp_filename[PATH_MAX +7];
  FILE *fp;
  FILE *fptmp;
  size_t n;
  va_list argp;
  char new_headers[2048];

  va_start(argp, add_headers);
  vsnprintf(new_headers, sizeof(new_headers), add_headers, argp);
  va_end(argp);

  // 3.1.12: Temporary file in checked directory causes troubles with more than one modems:
  //sprintf(tmp_filename,"%s.XXXXXX", filename);
  sprintf(tmp_filename,"/tmp/smsd.XXXXXX");

  close(mkstemp(tmp_filename));
  unlink(tmp_filename);
  if (!(fptmp = fopen(tmp_filename, "w")))
  {
    writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, creating %s failed", tmp_filename));
    alarm_handler0(LOG_WARNING, tb);
    result = 1;
  }
  else
  {
    if (!(fp = fopen(filename, "r")))
    {
      fclose(fptmp);
      unlink(tmp_filename);
      writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, reading %s failed", filename));
      alarm_handler0(LOG_WARNING, tb);
      result = 2;
    }
    else
    {
      strcpy(header, "\n");
      while (in_headers && fgets(line, sizeof(line), fp))
      {
        if (remove_headers && *remove_headers)
        {
          // Possible old headers are removed:
          if ((p = strchr(line, ':')))
          {
            strncpy(header +1, line, p -line +1);
            header[p -line +2] = 0;
            if (strstr(remove_headers, header))
              continue;
          }
        }

        if (line_is_blank(line))
        {
          if (*new_headers)
            fwrite(new_headers, 1, strlen(new_headers), fptmp);
          if (add_buffer && *add_buffer)
            fwrite(add_buffer, 1, strlen(add_buffer), fptmp);
          in_headers = 0;
        }
        fwrite(line, 1, strlen(line), fptmp);
      }

      // 3.1beta7: Because of Include feature, all text can be in different file
      // and therefore a delimiter line is not in this file.
      if (in_headers)
      {
        if (*new_headers)
          fwrite(new_headers, 1, strlen(new_headers), fptmp);
        if (add_buffer && *add_buffer)
          fwrite(add_buffer, 1, strlen(add_buffer), fptmp);
      }

      while ((n = fread(line, 1, sizeof(line), fp)) > 0)
        fwrite(line, 1, n, fptmp);

      fclose(fptmp);
      fclose(fp);

      // 3.1.14: rename does not work across different mount points:
      //unlink(filename);
      //rename(tmp_filename, filename);
      if (rename(tmp_filename, filename) != 0)
      {
        if (!(fptmp = fopen(tmp_filename, "r")))
        {
            writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, reading %s failed", tmp_filename));
            alarm_handler0(LOG_WARNING, tb);
            result = 2;
        }
        else
        {
          if (!(fp = fopen(filename, "w")))
          {
            writelogfile0(LOG_WARNING, 1, tb_sprintf("Header handling aborted, creating %s failed", filename));
            alarm_handler0(LOG_WARNING, tb);
            result = 1;
          }
          else
          {
            while ((n = fread(line, 1, sizeof(line), fptmp)) > 0)
              fwrite(line, 1, n, fp);

            fclose(fp);
          }

          fclose(fptmp);
          unlink(tmp_filename);
        }
      }

    }
  }

  return result;
}
