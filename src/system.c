
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

int exec_system(char *command, char *info)
{
  int result;
  int *i = 0;
  char *to = NULL;
  //static int last_status_eventhandler = -1;
  //static int last_status_rr_modem = -1;
  //static int last_status_rr_post_modem = -1;
  //static int last_status_rr_mainprocess = -1;
  static int last_status = -1; // One status for each process

  result = my_system(command, info);

  if (!strcmp(info, EXEC_EVENTHANDLER))
    i = &last_status; //_eventhandler;
  else
  if (!strcmp(info, EXEC_RR_MODEM))
    i = &last_status; //_rr_modem;
  else
  if (!strcmp(info, EXEC_RR_POST_MODEM))
    i = &last_status; //_rr_post_modem;
  else
  if (!strcmp(info, EXEC_RR_MAINPROCESS))
    i = &last_status; //_rr_mainprocess;

  if (i)
  {
    if (!result)
      *i = 0; // running was ok
    else
    {
      char alert[256];

      snprintf(alert, sizeof(alert), "problem with %s, result %i", info, result);

      if (process_id >= 0)
      {
        // Modems only.
        if (DEVICE.admin_to[0])
          to = DEVICE.admin_to;
        else if (admin_to[0])
          to = admin_to;
      }

      // If last result was ok or unknown, alert is sent.
      if (*i <= 0)
      {
        int timeout = 0;

        writelogfile(LOG_ERR, 1, "ALERT: %s", alert);

        if (to)
        {
          char msg[256];
          int quick = 0;
          int errorcounter = 0;

          snprintf(msg, sizeof(msg), "Smsd3: %s, %s", DEVICE.name, alert);
          send_admin_message(&quick, &errorcounter, msg);
        }
        else
        {
          if (*adminmessage_device && process_id == -1 && shared_buffer)
          {
            time_t start_time;

            start_time = time(0);

            // Buffer should be free:
            while (*shared_buffer)
            {
              if (time(0) -start_time > 60)
                break;
              sendsignal2devices(SIGCONT);
              t_sleep(5);
            }

            if (*shared_buffer)
            {
              timeout = 1;
              writelogfile(LOG_INFO, 1, "Timeout while trying to deliver alert to %s", adminmessage_device);
            }
            else
            {
              snprintf(shared_buffer, SIZE_SHARED_BUFFER, "%s Sms3: mainprocess, %s", adminmessage_device, alert);
              sendsignal2devices(SIGCONT);
            }
          }
        }

        if (timeout)
          *i = -1; // retry next time if error remains
        else
          *i = 1; // running failed
      }
      else
      {
        (*i)++;
        writelogfile(LOG_INFO, 1, "ALERT (continues, %i): %s", *i, alert);
      }
    }
  }

  return result;
}
