#include "readSMS.h"

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
#include "header_helpers.h"
#include "macro.h"

/* =======================================================================
   Read the header of an SMS file
   ======================================================================= */

void readSMSheader(char* filename, /* Filename */
                   int recursion_level,
// output variables are:
                   char* to, 		/* destination number */
             char* from, 		/* sender name or number */
             int*  alphabet, 	/* -1=GSM 0=ISO 1=binary 2=UCS2 3=UTF8 4=unknown */
                   int* with_udh,  	/* UDH flag */
                   char* udh_data,  	/* UDH data in hex dump format. Only used in alphabet<=0 */
             char* queue, 	/* Name of Queue */
             int*  flash, 	/* 1 if send as Flash SMS */
             char* smsc, 		/* SMSC Number */
                   int*  report,  	/* 1 if request status report */
       int*  split,  	/* 1 if request splitting */
                   int*  validity, 	/* requested validity period value */
                   int*  voicecall, 	/* 1 if request voicecall */
                   int* hex,		/* 1 if binary message is presented as hexadecimal */
                   int *replace_msg,	/* 1...7, 0=none */
                   char *macros,
                   int *system_msg,	/* 1 if sending as a system message. */
                   int *to_type)        /* -1 = default, -2 = error, >= 0 = accepted value  */
{
  FILE* File;
  char line[SIZE_UDH_DATA +256];
  char *ptr;
  int hlen;

  if (recursion_level == 0)
  {
    to[0] = 0;
    from[0] = 0;
    *alphabet = 0;
    *with_udh = -1;
    udh_data[0] = 0;
    queue[0] = 0;
    *flash = 0;
    smsc[0] = 0;
    *report = -1;
    *split = -1;
    *validity = -1;
    *voicecall = 0;
    *hex = 0;
    if (macros)
      *macros = 0;
    *system_msg = 0;
    *to_type = -1;

    // This is global:
    smsd_debug[0] = 0;
  }

#ifdef DEBUGMSG
  printf("!! readSMSheader(filename=%s, recursion_level:%i, ...)\n",filename, recursion_level);
#endif

  if ((File = fopen(filename, "r")))
  {
    // read until end of file or until an empty line was found
    while (fgets(line,sizeof(line),File))
    {
      if (line_is_blank(line))
        break;

      if (test_header(&hlen, line, HDR_To, HDR_To2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_TO)
        {
          // correct phone number if it has wrong syntax
          if (strstr(line,"00")==line)
            strcpy(to,line+2);
          else if ((ptr=strchr(line,'+')))
            strcpy(to,ptr+1);
          else
            strcpy(to,line);

          // 3.1beta3:
          // Allow number grouping (like "358 12 345 6789") and *# characters in any position of a phone number.
          ptr = (*to == 's')? to +1: to;
          while (*ptr)
          {
            // 3.1.1: Allow -
            if (is_blank(*ptr) || *ptr == '-')
              strcpyo(ptr, ptr +1);
            else if (!strchr("*#0123456789", *ptr))
              *ptr = 0;
            else
              ptr++;
          }
        }

        // 3.1.6: Cannot accept 's' only:
        if (!strcmp(to, "s"))
                *to = 0;
      }
      else
      if (test_header(&hlen, line, HDR_ToTOA, HDR_ToTOA2))
      {
        cutspaces(strcpyo(line, line +hlen));

        if (!strcasecmp(line, "unknown"))
          *to_type = 0;
        else if (!strcasecmp(line, "international"))
          *to_type = 1;
        else if (!strcasecmp(line, "national"))
          *to_type = 2;
        else
          *to_type = -2;
      }
      else
      if (test_header(&hlen, line, HDR_From, HDR_From2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_FROM)
          strcpy(from, line);
      }
      else
      if (test_header(&hlen, line, HDR_SMSC, HDR_SMSC2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_SMSC)
        {
          // 3.1beta7: allow grouping:
          // 3.1.5: fixed infinite loop bug.
          ptr = line;
          while (*ptr)
          {
            if (is_blank(*ptr))
              strcpyo(ptr, ptr +1);
            else
              ptr++;
          }

          // correct phone number if it has wrong syntax
          if (strstr(line,"00")==line)
            strcpy(smsc,line+2);
          else if (strchr(line,'+')==line)
            strcpy(smsc,line+1);
          else
            strcpy(smsc,line);
        }
      }
      else
      if (test_header(&hlen, line, HDR_Flash, HDR_Flash2))
        *flash = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Provider, HDR_Provider2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_QUEUENAME)
          strcpy(queue, line);
      }
      else
      if (test_header(&hlen, line, HDR_Queue, HDR_Queue2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_QUEUENAME)
          strcpy(queue, line);
      }
      else
      if (test_header(&hlen, line, HDR_Binary, HDR_Binary2))
        *alphabet = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Report, HDR_Report2))
        *report = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Autosplit, HDR_Autosplit2))
        *split = atoi(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Validity, HDR_Validity2))
        *validity = parse_validity(cutspaces(strcpyo(line, line +hlen)), *validity);
      else
      if (test_header(&hlen, line, HDR_Voicecall, HDR_Voicecall2))
        *voicecall = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Hex, HDR_Hex2))
        *hex = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Replace, HDR_Replace2))
      {
        *replace_msg = atoi(cutspaces(strcpyo(line, line +hlen)));
        if (*replace_msg < 0 || *replace_msg > 7)
          *replace_msg = 0;
      }
      else
      if (test_header(&hlen, line, HDR_Alphabet, HDR_Alphabet2))
      {
        cutspaces(strcpyo(line, line +hlen));
        if (strcasecmp(line,"GSM")==0)
          *alphabet=-1;
        else if (strncasecmp(line,"iso",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"lat",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"ans",3)==0)
          *alphabet=0;
        else if (strncasecmp(line,"bin",3)==0)
          *alphabet=1;
        else if (strncasecmp(line,"chi",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"ucs",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"uni",3)==0)
          *alphabet=2;
        else if (strncasecmp(line,"utf",3)==0)
          *alphabet=3;
        else
          *alphabet=4;
      }
      else
      if (strncmp(line, HDR_UDHDATA, hlen = strlen(HDR_UDHDATA)) == 0)
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_UDH_DATA)
          strcpy(udh_data, line);
      }
      else
      if (strncmp(line, HDR_UDHDUMP, hlen = strlen(HDR_UDHDUMP)) == 0) // same as UDH-DATA for backward compatibility
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_UDH_DATA)
          strcpy(udh_data, line);
      }
      else
      if (strncmp(line, HDR_UDH, hlen = strlen(HDR_UDH)) == 0)
        *with_udh = yesno(cutspaces(strcpyo(line, line +hlen)));
      else
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        cutspaces(strcpyo(line, line +hlen));
        if (recursion_level < 1)
          readSMSheader(line, recursion_level +1, to, from, alphabet, with_udh, udh_data, queue, flash,
                  smsc, report, split, validity, voicecall, hex, replace_msg, macros, system_msg, to_type);
      }
      else
      if (test_header(&hlen, line, HDR_Macro, HDR_Macro2))
      {
        char *p, *p1, *p2;

        cut_ctrl(strcpyo(line, line +hlen));
        while (*line == ' ' || *line == '\t')
          strcpyo(line, line +1);

        if ((p1 = strchr(line, '=')) && *line != '=' && macros)
        {
          // If there is previously defined macro with the same name, it is removed first:
          p = macros;
          while (*p)
          {
            if (strncmp(p, line, 1 +(int)(p1 -line)) == 0)
            {
              p2 = strchr(p, 0) +1;
              memmove(p, p2, SIZE_MACROS -(p2 -macros));
              break;
            }
            p = strchr(p, 0) +1;
          }

          p = macros;
          while (*p)
            p = strchr(p, 0) +1;
          if ((ssize_t)strlen(line) <= SIZE_MACROS -2 -(p - macros))
          {
            strcpy(p, line);
            *(p +strlen(line) +1) = 0;
          }
          // No space -error is not reported.
        }
      }
      else
      if (test_header(&hlen, line, HDR_System_message, HDR_System_message2))
      {
        // 3.1.7:
        // *system_msg = yesno(cutspaces(strcpyo(line, line +hlen)));
        cutspaces(strcpyo(line, line + hlen));
        if (!strcasecmp(line, "ToSIM") || !strcmp(line, "2"))
          *system_msg = 2;
        else
          *system_msg = yesno(line);
      }
      else
      if (test_header(&hlen, line, HDR_Smsd_debug, HDR_Smsd_debug2))
      {
        if (strlen(cutspaces(strcpyo(line, line +hlen))) < SIZE_SMSD_DEBUG)
          strcpy(smsd_debug, line); // smsd_debug is global.
      }
      else // if the header is unknown, then simply ignore
      {
        ;;
      }
    }    // End of header reached
    fclose(File);
#ifdef DEBUGMSG
  printf("!! to=%s\n",to);
  printf("!! from=%s\n",from);
  printf("!! alphabet=%i\n",*alphabet);
  printf("!! with_udh=%i\n",*with_udh);
  printf("!! udh_data=%s\n",udh_data);
  printf("!! queue=%s\n",queue);
  printf("!! flash=%i\n",*flash);
  printf("!! smsc=%s\n",smsc);
  printf("!! report=%i\n",*report);
  printf("!! split=%i\n",*split);
  printf("!! validity=%i\n",*validity);
  if (recursion_level == 0 && macros)
  {
    char *p = macros;

    printf("!! %s\n", (*p)? "macros:" : "no macros");
    while (*p)
    {
      printf ("%s\n", p);
      p = strchr(p, 0) +1;
    }
  }
#endif
  }
  else
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.",filename));
    alarm_handler0(LOG_ERR, tb);
  }
}


/* =======================================================================
   Read the message text or binary data of an SMS file
   ======================================================================= */

void readSMStext(char* filename, /* Filename */
                 int recursion_level,
                 int do_convert, /* shall I convert from ISO to GSM? Do not try to convert binary data. */
// output variables are:
                 char* text,     /* message text */
                 int* textlen,   /* text length */
                 char *macros)
{
  FILE *fp;
  int in_headers = 1;
  char line[MAXTEXT +1]; // 3.1beta7: We now need a 0-termination for extract_macros.
  int n;
  int hlen;

  if (recursion_level == 0)
  {
    // Initialize result with empty string
    text[0]=0;
    *textlen=0;
  }

#ifdef DEBUGMSG
  printf("readSMStext(filename=%s, recursion_level=%i, do_convert=%i, ...)\n",filename, recursion_level, do_convert);
#endif

  if (!(fp = fopen(filename, "r")))
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.",filename));
    alarm_handler0(LOG_ERR, tb);
  }
  else
  {
    while (in_headers && fgets(line, sizeof(line), fp))
    {
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        strcpyo(line, line +hlen);
        cutspaces(line);
        if (recursion_level < 1)
          readSMStext(line, recursion_level +1, do_convert, text, textlen, macros);
      }

      if (line_is_blank(line))
        in_headers = 0;
    }

    n = fread(line, 1, sizeof(line) -1, fp);
    fclose(fp);

    if (n > 0)
    {
      // Convert character set or simply copy
      if (do_convert == 1)
      {
        if (macros && *macros)
        {
          line[n] = 0;
          extract_macros(line, sizeof(line), macros);
          n = strlen(line);
        }

        // 3.1.7:
        if (trim_text)
        {
          // 3.1.9: do not trim if it becomes empty:
          char *saved_line;
          int saved_n;

          line[n] = 0;
          saved_line = strdup(line);
          saved_n = n;

          while (n > 0)
          {
            if (line[n - 1] && strchr(" \t\n\r", line[n - 1]))
            {
              n--;
              line[n] = 0;
            }
            else
              break;
          }

          if (!(*line))
          {
            strcpy(line, saved_line);
            n = saved_n;
          }
          free(saved_line);
        }

        *textlen += iso_utf8_2gsm(line, n, text + *textlen, MAXTEXT - *textlen);
      }
      else
      {
        memmove(text + *textlen, line, n);
        *textlen += n;
      }
    }
  }

#ifdef DEBUGMSG
  printf("!! textlen=%i\n",*textlen);
#endif
}

void readSMShex(char *filename, int recursion_level, char *text, int *textlen, char *macros, char *errortext)
{
  FILE *fp;
  char line[MAXTEXT +1];
  int in_headers = 1;
  char *p;
  int h;
  int i = 0;
  char *p_length = NULL;
  int too_long = 0;
  int hlen;

  if (recursion_level == 0)
  {
    text[0]=0;
    *textlen=0;
  }

  if ((fp = fopen(filename, "r")))
  {
    while (in_headers && fgets(line, sizeof(line), fp))
    {
      if (test_header(&hlen, line, HDR_Include, HDR_Include2))
      {
        strcpyo(line, line +hlen);
        cutspaces(line);
        if (recursion_level < 1)
          readSMShex(line, recursion_level +1, text, textlen, macros, errortext);
      }

      if (line_is_blank(line))
        in_headers = 0;
    }

    while (fgets(line, sizeof(line), fp) && !too_long)
    {
      cut_ctrl(line);
      while (*line == ' ' || *line == '\t')
        strcpyo(line, line +1);

      extract_macros(line, sizeof(line), macros);

      if (strncmp(line, "INLINESTRING:", 13) == 0)
      {
        if (*textlen +strlen(line) +1 -13 +1 >= MAXTEXT)
        {
          too_long = 1;
          break;
        }
        // Inline String follows:
        text[*textlen] = 0x03;
        (*textlen)++;
        // Actual text:
        strcpy(text + *textlen, line +13);
        *textlen += strlen(line) -13;
        // Termination:
        text[*textlen] = 0x00;
        (*textlen)++;
      }
      else
      if (strncmp(line, "STRING:", 7) == 0)
      {
        if (*textlen +strlen(line) -7 >= MAXTEXT)
        {
          too_long = 1;
          break;
        }
        strcpy(text + *textlen, line +7);
        *textlen += strlen(line) -7;
      }
      else
      if (strncmp(line, "LENGTH", 6) == 0)
      {
        if (!p_length)
        {
          if (*textlen +1 >= MAXTEXT)
          {
            too_long = 1;
            break;
          }
          p_length = text + *textlen;
          (*textlen)++;
        }
        else
        {
          *p_length = text + *textlen - p_length -1;
          p_length = NULL;
        }
      }
      else
      {
        if ((p = strstr(line, "/")))
          *p = 0;
        if ((p = strstr(line, "'")))
          *p = 0;
        if ((p = strstr(line, "#")))
          *p = 0;
        if ((p = strstr(line, ":")))
          *p = 0;
        while ((p = strchr(line, ' ')))
          strcpyo(p, p +1);

        if (*line)
        {
          if (strlen(line) % 2 != 0)
          {
            writelogfile0(LOG_ERR, 1, tb_sprintf("Hex presentation error in sms file %s: incorrect length of data: \"%s\".", filename, line));
            alarm_handler0(LOG_ERR, tb);
            if (errortext)
              strcpy(errortext, tb);
            text[0]=0;
            *textlen=0;
            break;
          }

          p = line;
          while (*p)
          {
            if ((i = sscanf(p, "%2x", &h)) == 0)
              break;
            if (*textlen +1 >= MAXTEXT)
            {
              too_long = 1;
              break; // Main loop is breaked by too_long variable.
            }
            text[*textlen] = h;
            (*textlen)++;
            p += 2;
          }

          if (i < 1)
          {
            writelogfile0(LOG_ERR, 1, tb_sprintf("Hex conversion error in sms file %s: \"%s\".", filename, p));
            alarm_handler0(LOG_ERR, tb);
            if (errortext)
              strcpy(errortext, tb);
            text[0]=0;
            *textlen=0;
            break;
          }
        }
      }
    }

    fclose(fp);

    if (p_length)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("LENGTH termination error in sms file %s.", filename));
      alarm_handler0(LOG_ERR, tb);
      if (errortext)
        strcpy(errortext, tb);
      text[0]=0;
      *textlen=0;
    }

    if (too_long)
    {
      writelogfile0(LOG_ERR, 1, tb_sprintf("Data is too long in sms file %s.", filename));
      alarm_handler0(LOG_ERR, tb);
      if (errortext)
        strcpy(errortext, tb);
      text[0]=0;
      *textlen=0;
    }
  }
  else
  {
    writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot read sms file %s.", filename));
    alarm_handler0(LOG_ERR, tb);
    if (errortext)
      strcpy(errortext, tb);
  }

  // No need to show filename in the message file:
  if (errortext)
    if ((p = strstr(errortext, filename)))
      strcpyo(p -1, p +strlen(filename));
}
