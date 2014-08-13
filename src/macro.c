
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

/* =======================================================================
   Macros can be used with ISO/UTF-8 written messages with cs_convert=yes
   and with binary messages written with hex=yes.
   A buffer should be 0-terminated and it cannot include 0x00 characters.
   ======================================================================= */

int extract_macros(char *buffer, int buffer_size, char *macros)
{
  int result = 0;
  char *p_buffer;
  char *p_macro;
  char *p_value;
  char *p;
  int len_buffer;
  int len_macro;
  int len_value;

  if (macros && *macros)
  {
    p_macro = macros;
    while (*p_macro)
    {
      if ((p_value = strchr(p_macro, '=')))
      {
        p_value++;
        *(p_value -1) = 0; // for easier use of strstr.
        len_macro = strlen(p_macro);
        len_value = strlen(p_value);
        p_buffer = buffer;
        while ((p = strstr(p_buffer, p_macro)))
        {
          if (len_macro < len_value)
          {
            len_buffer = strlen(buffer);
            if (len_buffer -len_macro +len_value >= buffer_size)
            {
              result = 1;
              break;
            }
            memmove(p +len_value, p +len_macro, len_buffer -(p -buffer) -len_macro +1);
          }
          else if (len_macro > len_value)
            strcpyo(p +len_value, p +len_macro);

          if (len_value > 0)
          {
            strncpy(p, p_value, len_value);
            p_buffer = p +len_value;
          }
        }

        *(p_value -1) = '='; // restore delimiter.
      }
      p_macro = strchr(p_macro, 0) +1;
    }
  }

  return result;
}
