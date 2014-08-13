#include "translation.h"

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

int read_translation(void)
{
  int result = 0; // Number of problems
  FILE *fp;
  char name[32];
  char value[PATH_MAX];
  int getline_result;
  char *p;
  int i;

  if (*language_file)
  {
    if (!(fp = fopen(language_file, "r")))
    {
      fprintf(stderr, "%s\n", tb_sprintf("Cannot read language file %s: %s", language_file, strerror(errno)));
      writelogfile(LOG_CRIT, 1, "%s", tb);
      result++;
    }
    else
    {
      while ((getline_result = my_getline(fp, name, sizeof(name), value, sizeof(value))) != 0)
      {
        if (getline_result == -1)
        {
          fprintf(stderr, "%s\n", tb_sprintf("Syntax Error in language file: %s", value));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (line_is_blank(value))
        {
          fprintf(stderr, "%s\n", tb_sprintf("%s has no value in language file.", name));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (strlen(value) >= SIZE_HEADER)
        {
          fprintf(stderr, "%s\n", tb_sprintf("Too long value for %s in language file: %s", name, value));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }

        if (*value == '-')
        {
          while (value[1] && strchr(" \t", value[1]))
            strcpyo(value +1, value +2);

          if (!strcasecmp(name, HDR_From) ||
              !strcasecmp(name, HDR_Received) ||
              !strcasecmp(name, HDR_missed) ||
              !strcasecmp(name, HDR_missed_text))
          {
            fprintf(stderr, "%s\n", tb_sprintf("In language file, translation for %s cannot start with '-' character", name));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }
        }

        if (!strcasecmp(name, "incoming"))
          translate_incoming = yesno(value);
        else if (!strcasecmp(name, "datetime"))
        {
          // Not much can be checked, only if it's completelly wrong...
          char timestamp[81];
          time_t now;

          time(&now);
          if (!strchr(value, '%') ||
              strftime(timestamp, sizeof(timestamp), value, localtime(&now)) == 0 ||
              !strcmp(timestamp, value))
          {
            fprintf(stderr, "%s\n", tb_sprintf("In language file, format for datetime is completelly wrong: \"%s\"", value));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }
          else
            strcpy(datetime_format, value);
        }
        else if (!strcasecmp(name, "yes_word"))
          strcpy(yes_word, value);
        else if (!strcasecmp(name, "no_word"))
          strcpy(no_word, value);
        else if (!strcasecmp(name, "yes_chars") || !strcasecmp(name, "no_chars"))
        {
          // Every possible character (combination) is given between apostrophes.
          // This is because one UTF-8 character can be represented using more than on byte.
          // There can be more than one definition delimited with a comma. Uppercase and
          // lowercase is handled by the definition because of UTF-8 and because of
          // non-US-ASCII character sets.
          // Example (very easy one): 'K','k'

          // First remove commas, spaces and double apostrophes:
          while ((p = strchr(value, ',')))
            strcpyo(p, p +1);
          while ((p = strchr(value, ' ')))
            strcpyo(p, p +1);
          while ((p = strstr(value, "''")))
            strcpyo(p, p +1);

          // First apostrophe is not needed:
          if (value[0] == '\'')
            strcpyo(value, value +1);
          // Ensure that last apostrophe is there:
          if ((i = strlen(value)) > 0)
          {
            if (value[i -1] != '\'')
              if (i < SIZE_HEADER -1)
                strcat(value, "'");
          }
          else
          {
            fprintf(stderr, "%s\n", tb_sprintf("%s has an incomplete value in language file", name));
            writelogfile(LOG_CRIT, 1, "%s", tb);
            result++;
            continue;
          }

          // Example (UTF-8): (byte1)(byte2)(byte3)'(byte1)(byte2)'
          // yesno() is now able to check what was meant with an answer.

          if (!strcasecmp(name, "yes_chars"))
            strcpy(yes_chars, value);
          else
            strcpy(no_chars, value);
        }
        else if (!strcasecmp(name, HDR_To))
          strcpy(HDR_To2, value);
        else if (!strcasecmp(name, HDR_ToTOA))
          strcpy(HDR_ToTOA2, value);
        else if (!strcasecmp(name, HDR_From))
          strcpy(HDR_From2, value);
        else if (!strcasecmp(name, HDR_Flash))
          strcpy(HDR_Flash2, value);
        else if (!strcasecmp(name, HDR_Provider))
          strcpy(HDR_Provider2, value);
        else if (!strcasecmp(name, HDR_Queue))
          strcpy(HDR_Queue2, value);
        else if (!strcasecmp(name, HDR_Binary))
          strcpy(HDR_Binary2, value);
        else if (!strcasecmp(name, HDR_Report))
          strcpy(HDR_Report2, value);
        else if (!strcasecmp(name, HDR_Autosplit))
          strcpy(HDR_Autosplit2, value);
        else if (!strcasecmp(name, HDR_Validity))
          strcpy(HDR_Validity2, value);
        else if (!strcasecmp(name, HDR_Voicecall))
          strcpy(HDR_Voicecall2, value);
        else if (!strcasecmp(name, HDR_Replace))
          strcpy(HDR_Replace2, value);
        else if (!strcasecmp(name, HDR_Alphabet))
          strcpy(HDR_Alphabet2, value);
        else if (!strcasecmp(name, HDR_Include))
          strcpy(HDR_Include2, value);
        else if (!strcasecmp(name, HDR_Macro))
          strcpy(HDR_Macro2, value);
        else if (!strcasecmp(name, HDR_Hex))
          strcpy(HDR_Hex2, value);
        else if (!strcasecmp(name, HDR_SMSC))
          strcpy(HDR_SMSC2, value);
        else if (!strcasecmp(name, HDR_Priority))
          strcpy(HDR_Priority2, value);
        else if (!strcasecmp(name, HDR_System_message))
          strcpy(HDR_System_message2, value);
        else if (!strcasecmp(name, HDR_Sent))
          strcpy(HDR_Sent2, value);
        else if (!strcasecmp(name, HDR_Modem))
          strcpy(HDR_Modem2, value);
        else if (!strcasecmp(name, HDR_Number))
          strcpy(HDR_Number2, value);
        else if (!strcasecmp(name, HDR_FromTOA))
          strcpy(HDR_FromTOA2, value);
        else if (!strcasecmp(name, HDR_FromSMSC))
          strcpy(HDR_FromSMSC2, value);
        else if (!strcasecmp(name, HDR_Name))
          strcpy(HDR_Name2, value);
        else if (!strcasecmp(name, HDR_Received))
          strcpy(HDR_Received2, value);
        else if (!strcasecmp(name, HDR_Subject))
          strcpy(HDR_Subject2, value);
        else if (!strcasecmp(name, HDR_UDHType))
          strcpy(HDR_UDHType2, value);
        else if (!strcasecmp(name, HDR_Length))
          strcpy(HDR_Length2, value);
        else if (!strcasecmp(name, HDR_FailReason))
          strcpy(HDR_FailReason2, value);
        else if (!strcasecmp(name, HDR_Failed))
          strcpy(HDR_Failed2, value);
        else if (!strcasecmp(name, HDR_Identity))
          strcpy(HDR_Identity2, value);
        else if (!strcasecmp(name, HDR_MessageId))
          strcpy(HDR_MessageId2, value);
        else if (!strcasecmp(name, HDR_OriginalFilename))
          strcpy(HDR_OriginalFilename2, value);
        else if (!strcasecmp(name, HDR_CallType))
          strcpy(HDR_CallType2, value);
        else if (!strcasecmp(name, HDR_missed))
          strcpy(HDR_missed2, value);
        else if (!strcasecmp(name, HDR_missed_text))
          strcpy(HDR_missed_text2, value);
        else if (!strcasecmp(name, HDR_Result))
          strcpy(HDR_Result2, value);
        else if (!strcasecmp(name, HDR_Incomplete))
          strcpy(HDR_Incomplete2, value);
        else
        {
          fprintf(stderr, "%s\n", tb_sprintf("Unknown variable in language file: %s", name));
          writelogfile(LOG_CRIT, 1, "%s", tb);
          result++;
          continue;
        }
      }

      fclose(fp);
    }

    if (!result)
      writelogfile(LOG_INFO, 0, "Using language file %s", language_file);
  }

  return result;
}
