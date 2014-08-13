
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
#include "extras.h"
#include "locking.h"
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

/* =======================================================================
   Write a received message into a file
   ======================================================================= */

// returns 1 if this was a status report
int received2file(char *line1, char *line2, char *filename, int *stored_concatenated, int incomplete)
{
  int userdatalength;
  char ascii[MAXTEXT]= {};
  char sendr[100]= {};
  int with_udh=0;
  char udh_data[SIZE_UDH_DATA] = {};
  char udh_type[SIZE_UDH_TYPE] = {};
  char smsc[31]= {};
  char name[64]= {};
  char date[9]= {};
  char Time[9]= {};
  char warning_headers[SIZE_WARNING_HEADERS] = {};
  //char status[40]={}; not used
  int alphabet=0;
  int is_statusreport=0;
  FILE* fd;
  int do_decode_unicode_text = 0;
  int do_internal_combine = 0;
  int do_internal_combine_binary;
  int is_unsupported_pdu = 0;
  int pdu_store_length = 0;
  int result = 1;
  char from_toa[51] = {};
  int report;
  int replace;
  int flash;
  int p_count = 1;
  int p_number;
  char sent[81] = "";
  char received[81] = "";

  if (DEVICE.decode_unicode_text == 1 ||
      (DEVICE.decode_unicode_text == -1 && decode_unicode_text == 1))
    do_decode_unicode_text = 1;

  if (!incomplete)
    if (DEVICE.internal_combine == 1 ||
        (DEVICE.internal_combine == -1 && internal_combine == 1))
      do_internal_combine = 1;

  do_internal_combine_binary = do_internal_combine;
  if (do_internal_combine_binary)
    if (DEVICE.internal_combine_binary == 0 ||
        (DEVICE.internal_combine_binary == -1 && internal_combine_binary == 0))
      do_internal_combine_binary = 0;

#ifdef DEBUGMSG
  printf("!! received2file: line1=%s, line2=%s, decode_unicode_text=%i, internal_combine=%i, internal_combine_binary=%i\n",
         line1, line2, do_decode_unicode_text, do_internal_combine, do_internal_combine_binary);
#endif

  //getfield(line1,1,status, sizeof(status)); not used
  getfield(line1,2,name, sizeof(name));

  // Check if field 2 was a number instead of a name
  if (atoi(name)>0)
    name[0]=0; //Delete the name because it is missing

  userdatalength=splitpdu(line2, DEVICE.mode, &alphabet, sendr, date, Time, ascii, smsc, &with_udh, udh_data, udh_type,
                          &is_statusreport, &is_unsupported_pdu, from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);
  if (alphabet==-1 && DEVICE.cs_convert==1)
    userdatalength=gsm2iso(ascii,userdatalength,ascii,sizeof(ascii));
  else if (alphabet == 2 && do_decode_unicode_text == 1)
  {
    if (with_udh)
    {
      char *tmp;
      int m_id, p_count, p_number;

      if ((tmp = strdup(udh_data)))
      {
        if (get_remove_concatenation(tmp, &m_id, &p_count, &p_number) > 0)
        {
          if (p_count == 1 && p_number == 1)
          {
            strcpy(udh_data, tmp);
            if (!(*udh_data))
            {
              with_udh = 0;
              *udh_type = 0;
            }
            else
            {
              if (explain_udh(udh_type, udh_data) < 0)
                if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
                  sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");
            }
          }
        }
        free(tmp);
      }
    }

#ifndef USE_ICONV
    // 3.1beta7, 3.0.9: decoding is always done:
    userdatalength = decode_ucs2(ascii, userdatalength);
    alphabet = 0;
#else
    userdatalength = iconv_ucs2utf(ascii, userdatalength, sizeof(ascii));
    alphabet = 4;
#endif
  }

#ifdef DEBUGMSG
  printf("!! userdatalength=%i\n",userdatalength);
  printf("!! name=%s\n",name);
  printf("!! sendr=%s\n",sendr);
  printf("!! date=%s\n",date);
  printf("!! Time=%s\n",Time);
  if ((alphabet==-1 && DEVICE.cs_convert==1)||(alphabet==0)||(alphabet==4))
  printf("!! ascii=%s\n",ascii);
  printf("!! smsc=%s\n",smsc);
  printf("!! with_udh=%i\n",with_udh);
  printf("!! udh_data=%s\n",udh_data);
  printf("!! udh_type=%s\n",udh_type);
  printf("!! is_statusreport=%i\n",is_statusreport);
  printf("!! is_unsupported_pdu=%i\n", is_unsupported_pdu);
  printf("!! from_toa=%s\n", from_toa);
  printf("!! report=%i\n", report);
  printf("!! replace=%i\n", replace);
#endif

  if (is_statusreport)
  {
    char *p;
    char id[41];
    char status[128];
    const char SR_MessageId[] = "Message_id:"; // Fixed title inside the status report body.
    const char SR_Status[] = "Status:"; // Fixed title inside the status report body.

    *id = 0;
    *status = 0;
    if ((p = strstr(ascii, SR_MessageId)))
      sprintf(id, ", %s %i", SR_MessageId, atoi(p +strlen(SR_MessageId) +1));

    if ((p = strstr(ascii, SR_Status)))
      // 3.1.14: include explanation
      //sprintf(status, ", %s %i", SR_Status, atoi(p +strlen(SR_Status) +1));
      snprintf(status, sizeof(status), ", %s %s", SR_Status, p +strlen(SR_Status) +1);

    writelogfile(LOG_NOTICE, 0, "SMS received (report%s%s), From: %s", id, status, sendr);
  }

  *stored_concatenated = 0;
  if (do_internal_combine == 1)
  {
    int offset = 0; // points to the part count byte.
    int m_id;
    char storage_udh[SIZE_UDH_DATA] = {};
    int a_type;

    if ((a_type = get_concatenation(udh_data, &m_id, &p_count, &p_number)) > 0)
    {
      if (p_count > 1)
      {
        if (a_type == 1)
          sprintf(storage_udh, "05 00 03 ");
        else
          sprintf(storage_udh, "06 08 04 %02X ", (m_id & 0xFF00) >> 8);
        sprintf(strchr(storage_udh, 0), "%02X ", m_id & 0xFF);
        sprintf(strchr(storage_udh, 0), "%02X %02X ", p_count, p_number);
        offset = (a_type == 1)? 12 : 15;
      }
    }

    if (offset)
    {
      // This is a part of a concatenated message.
      char con_filename[PATH_MAX];
      //int partcount;
      char st[1024];
      char messageid[6];
      int i;
      int found = 0;
      int udlen;
      int ftmp;
      char tmp_filename[PATH_MAX];
      int cmp_start;
      int cmp_length;
      char *p;
      int part;
      struct stat statbuf;

      // First we store it to the concatenated store of this device:
      // 3.1beta7: Own folder for storage and smsd's other saved files:
      sprintf(con_filename, CONCATENATED_DIR_FNAME, (*d_saved)? d_saved : d_incoming, DEVICE.name);
      if (!(fd = fopen(con_filename, "a")))
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot open file %s: %s", con_filename, strerror(errno)));
        alarm_handler0(LOG_ERR, tb);
        result = 0;
      }
      else
      {
        //UDH-DATA: 05 00 03 02 03 02 PDU....
        //UDH-DATA: 06 08 04 00 02 03 02 PDU....
        fprintf(fd, "%s%s\n", storage_udh/*udh_data*/, line2);
        fclose(fd);
        //partcount = octet2bin(udh_data +offset);
        userdatalength = 0;
        *ascii = '\0';
        sprintf(messageid, (offset == 12)? "%.2s" : "%.5s", storage_udh/*udh_data*/ + 9);
        i = octet2bin(line2);
        cmp_length = octet2bin(line2 +2 +i*2 +2);
        if (cmp_length%2 != 0)
          cmp_length++;
        // 3.1.5: fixed start position of compare:
        //cmp_start = 2 +i*2 +4;
        cmp_start = 2 +i*2 +6;

#ifdef DEBUGMSG
  printf("!! --------------------\n");
  printf("!! storage_udh=%s\n", storage_udh);
  printf("!! line2=%.50s...\n", line2);
  printf("!! messageid=%s\n", messageid);
  printf("!! cmp_start=%i\n", cmp_start);
  printf("!! cmp_length=%i\n", cmp_length);
  printf("!!\n");
#endif

        // Next we try to find each part, starting at the first one:
        fd = fopen(con_filename, "r");
        for (i = 1; i <= p_count/*partcount*/; i++)
        {
          found = 0;
          fseek(fd, 0, SEEK_SET);
          while (fgets(st, sizeof(st), fd))
          {
            p = st +(octet2bin(st) +1) *3;
            part = (strncmp(st +3, "00", 2) == 0)? octet2bin(st +15) : octet2bin(st +18);
            if (strncmp(st +9, messageid, strlen(messageid)) == 0 && part == i &&
                strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0)
            {
              found = 1;
              pdu_store_length += strlen(p) +5;
              break;
            }
          }

          // If some part was not found, we can take a break.
          if (!found)
            break;
        }

        if (!found)
        {
          fclose(fd);
          *stored_concatenated = 1;
        }
        else
        {
          incoming_pdu_store = (char *)malloc(pdu_store_length +1);
          if (incoming_pdu_store)
            *incoming_pdu_store = 0;

          for (i = 1; i <= p_count/*partcount*/; i++)
          {
            fseek(fd, 0, SEEK_SET);
            while (fgets(st, sizeof(st), fd))
            {
              p = st +(octet2bin(st) +1) *3;
              part = (strncmp(st +3, "00", 2) == 0)? octet2bin(st +15) : octet2bin(st +18);
              if (strncmp(st +9, messageid, strlen(messageid)) == 0 && part == i &&
                  strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0)
              {
                if (incoming_pdu_store)
                {
                  strcat(incoming_pdu_store, "PDU: ");
                  strcat(incoming_pdu_store, p);
                }
                // Correct part was found, concatenate it's text to the buffer:
                if (i == 1) // udh_data and _type are taken from the first part only.
                  udlen = splitpdu(p, DEVICE.mode, &alphabet, sendr, date, Time, ascii +userdatalength, smsc,
                                   &with_udh, udh_data, udh_type, &is_statusreport, &is_unsupported_pdu,
                                   from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);
                else
                  udlen = splitpdu(p, DEVICE.mode, &alphabet, sendr, date, Time, ascii +userdatalength, smsc,
                                   &with_udh, 0, 0, &is_statusreport, &is_unsupported_pdu,
                                   from_toa, &report, &replace, warning_headers, &flash, do_internal_combine_binary);

                if (alphabet==-1 && DEVICE.cs_convert==1)
                  udlen=gsm2iso(ascii +userdatalength,udlen,ascii +userdatalength,sizeof(ascii) -userdatalength);
                else if (alphabet == 2 && do_decode_unicode_text == 1)
                {
#ifndef USE_ICONV
                  udlen = decode_ucs2(ascii +userdatalength, udlen);
                  alphabet = 0;
#else
                  udlen = iconv_ucs2utf(ascii +userdatalength, udlen,
                                        sizeof(ascii) -userdatalength);
                  alphabet = 4;
#endif
                }
                userdatalength += udlen;
                break;
              }
            }
          }

          sprintf(tmp_filename,"%s/%s.XXXXXX",d_incoming,DEVICE.name);
          ftmp = mkstemp(tmp_filename);
          fseek(fd, 0, SEEK_SET);
          while (fgets(st, sizeof(st), fd))
          {
            p = st +(octet2bin(st) +1) *3;
            if (!(strncmp(st +9, messageid, strlen(messageid)) == 0 &&
                strncmp(p +cmp_start, line2 +cmp_start, cmp_length) == 0))
              //write(ftmp, &st, strlen(st));
              write(ftmp, st, strlen(st));
          }

          close(ftmp);
          fclose(fd);
          unlink(con_filename);
          rename(tmp_filename, con_filename);

          // 3.1beta7: If a file is empty now, remove it:
          if (stat(con_filename, &statbuf) == 0)
            if (statbuf.st_size == 0)
              unlink(con_filename);

          // UDH-DATA is not valid anymore:
          // *udh_data = '\0';
          // with_udh = 0;
          if (remove_concatenation(udh_data) > 0)
          {
            if (!(*udh_data))
            {
              with_udh = 0;
              *udh_type = 0;
            }
            else
            {
              if (explain_udh(udh_type, udh_data) < 0)
                if (strlen(udh_type) +7 < SIZE_UDH_TYPE)
                  sprintf(strchr(udh_type, 0), "%sERROR", (*udh_type)? ", " : "");
            }
          }
        } // if, all parts were found.
      }
    } // if (offset), received message had concatenation header with more than 1 parts.
  } // do_internal_combine ends.

  if (p_count == 1)
  {
    if (!is_statusreport)
      writelogfile(LOG_NOTICE, 0, "SMS received, From: %s", sendr);
  }
  else
    writelogfile(LOG_NOTICE, 0, "SMS received (part %i/%i), From: %s", p_number, p_count, sendr);

  if (result)
  {
    if (*stored_concatenated)
      result = 0;
    else
    {
      // 3.1:
      //sprintf(filename, "%s/%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name);
      char timestamp[81];

      make_datetime_string(timestamp, sizeof(timestamp), 0, 0, date_filename_format);
      if (date_filename == 1)
        sprintf(filename, "%s/%s.%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, timestamp, DEVICE.name);
      else if (date_filename == 2)
        sprintf(filename, "%s/%s.%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name, timestamp);
      else
        sprintf(filename, "%s/%s.XXXXXX", (is_statusreport && *d_report)? d_report : d_incoming, DEVICE.name);

      close(mkstemp(filename));
      //Replace the temp file by a new file with same name. This resolves wrong file permissions.
      unlink(filename);
      if ((fd = fopen(filename, "w")))
      {
        char *p;

        // 3.1beta7, 3.0.9: This header can be used to detect that a message has no usual content:
        if (is_unsupported_pdu)
          fprintf(fd, "Error: Cannot decode PDU, see text part for details.\n");
        if (*warning_headers)
          fprintf(fd, "%s", warning_headers);
        fprintf(fd, "%s %s\n", get_header_incoming(HDR_From, HDR_From2), sendr);
        if (*from_toa && *HDR_FromTOA2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_FromTOA, HDR_FromTOA2), from_toa);
        if (name[0] && *HDR_Name2 != '-')
          fprintf(fd,"%s %s\n", get_header_incoming(HDR_Name, HDR_Name2), name);
        if (smsc[0] && *HDR_FromSMSC2 != '-')
          fprintf(fd,"%s %s\n", get_header_incoming(HDR_FromSMSC, HDR_FromSMSC2), smsc);

        if (date[0] && Time[0] && *HDR_Sent2 != '-')
        {
          make_datetime_string(sent, sizeof(sent), date, Time, 0);
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Sent, HDR_Sent2), sent);
        }

        // Add local timestamp
        make_datetime_string(received, sizeof(received), 0, 0, 0);
        fprintf(fd, "%s %s\n", get_header_incoming(HDR_Received, HDR_Received2), received);

        if (*HDR_Subject2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Subject, HDR_Subject2), DEVICE.name);

        // 3.1.4:
        if (*HDR_Modem2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Modem, HDR_Modem2), DEVICE.name);
        if (DEVICE.number[0])
          if (*HDR_Number2 != '-')
            fprintf(fd, "%s %s\n", get_header_incoming(HDR_Number, HDR_Number2), DEVICE.number);

        if (!strstr(DEVICE.identity, "ERROR") && *HDR_Identity2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Identity, HDR_Identity2), DEVICE.identity);
        if (report >= 0 && *HDR_Report2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Report, HDR_Report2), (report)? yes_word : no_word);
        if (replace >= 0 && *HDR_Replace2 != '-')
          fprintf(fd, "%s %i\n", get_header_incoming(HDR_Replace, HDR_Replace2), replace);

        // 3.1: Flash message is now detected. Header printed only if flash was used.
        if (flash > 0 && *HDR_Flash2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Flash, HDR_Flash2), yes_word);

        if (incomplete > 0 && *HDR_Incomplete2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Incomplete, HDR_Incomplete2), yes_word);

        p = "";
        if (alphabet == -1)
        {
          if (DEVICE.cs_convert)
            p = "ISO";
          else
            p = "GSM";
        }
        else if (alphabet == 0)
          p = "ISO";
        else if (alphabet == 1)
          p = "binary";
        else if (alphabet == 2)
          p = "UCS2";
        else if (alphabet == 4)
          p = "UTF-8";
        else if (alphabet == 3)
          p = "reserved";

        // 3.1.5:
        if (incoming_utf8 == 1 && alphabet <= 0)
          p = "UTF-8";

        if (*HDR_Alphabet2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_Alphabet, HDR_Alphabet2), p);

        if (udh_data[0])
          fprintf(fd,"%s %s\n", HDR_UDHDATA, udh_data);
        if (udh_type[0] && *HDR_UDHType2 != '-')
          fprintf(fd, "%s %s\n", get_header_incoming(HDR_UDHType, HDR_UDHType2), udh_type);

        // 3.1beta7: new header:
        if (*HDR_Length2 != '-')
          fprintf(fd, "%s %i\n", get_header_incoming(HDR_Length, HDR_Length2), (alphabet == 2)? userdatalength /2 : userdatalength);

        // 3.1beta7: This header is only necessary with binary messages. With other message types
        // there is UDH-DATA header included if UDH is presented. "true" / "false" is now
        // presented as "yes" / "no" which may be translated to some other language.
        if (alphabet == 1)
          fprintf(fd, "%s %s\n", HDR_UDH, (with_udh)? yes_word : no_word);

        // 3.1beta7, 3.0.9: with value 2 unsupported pdu's were not stored.
        if (store_received_pdu == 3 ||
           (store_received_pdu == 2 && (alphabet == 1 || alphabet == 2)) ||
           (store_received_pdu >= 1 && is_unsupported_pdu == 1))
        {
          if (incoming_pdu_store)
            fprintf(fd,"%s", incoming_pdu_store);
          else
            fprintf(fd,"PDU: %s\n", line2);
        }

        // Show the error position (first) if possible:
        if (store_received_pdu >= 1 && is_unsupported_pdu == 1)
        {
          char *p;
          char *p2;
          int pos;
          int len = 1;
          int i = 0;

          if ((p = strstr(ascii, "Position ")))
          {
            if ((pos = atoi(p +9)) > 0)
            {
              if ((p = strchr(p +9, ',')))
                if ((p2 = strchr(p, ':')) && p2 > p +1)
                  len = atoi(p +1);
              fprintf(fd, "Pos: ");
              while (i++ < pos -1)
                if (i % 10)
                {
                  if (i % 5)
                    fprintf(fd, ".");
                  else
                    fprintf(fd, "-");
                }
                else
                  fprintf(fd, "*");

              for (i = 0; i < len; i++)
                fprintf(fd, "^");
              fprintf(fd, "~here(%i)\n", pos);
            }
          }
        }
        // --------------------------------------------

        fprintf(fd,"\n");

        // UTF-8 conversion if necessary:
        if (incoming_utf8 == 1 && alphabet <= 0)
        {
          // 3.1beta7, 3.0.9: GSM alphabet is first converted to ISO
          if (alphabet == -1 && DEVICE.cs_convert == 0)
          {
            userdatalength = gsm2iso(ascii, userdatalength, ascii, sizeof(ascii));
            alphabet = 0; // filename_preview will need this information.
          }

          iso2utf8_file(fd, ascii, userdatalength);
        }
        else
          fwrite(ascii,1,userdatalength,fd);

        fclose(fd);

#ifdef DEBUGMSG
  if ((fd = fopen(filename, "r")))
  {
    char buffer[1024];

    printf("!! FILE: %s\n", filename);
    while (fgets(buffer, sizeof(buffer) -1, fd))
      printf("%s", buffer);
    fclose(fd);
  }
#endif

        if (filename_preview > 0)
        {
          char *text = NULL;
          char buffer[21];

          if (alphabet < 1)
          {
            if (alphabet == -1 && DEVICE.cs_convert == 0)
              userdatalength = gsm2iso(ascii, userdatalength, ascii, sizeof(ascii));
            text = ascii;
          }
          else if (alphabet == 1 || alphabet == 2)
          {
            strcpy(buffer, (alphabet == 1)? "binary" : "ucs2");
            text = buffer;
          }
          apply_filename_preview(filename, text, filename_preview);
        }

        writelogfile(LOG_INFO, 0, "Wrote an incoming %smessage file: %s", (incomplete)? "incomplete " : "", filename);

        result = is_statusreport; //return is_statusreport;
      }
      else
      {
        writelogfile0(LOG_ERR, 1, tb_sprintf("Cannot create file %s!", filename));
        alarm_handler0(LOG_ERR, tb);
        result = 0; //return 0;
      }
    }
  }

  if (incoming_pdu_store)
  {
    free(incoming_pdu_store);
    incoming_pdu_store = NULL;
  }
  return result;
}
