
#include "smsd_cfg.h"

#ifndef CONSTS_H
#define CONSTS_H

extern const char *HDR_To;		// Msg file input.
extern char HDR_To2[SIZE_HEADER];

extern const char *HDR_ToTOA;      // Msg file input. Type Of Address (numbering plan): unknown (0), international (1), national (2).
extern char HDR_ToTOA2[SIZE_HEADER];

extern const char *HDR_From;	// Msg file input: informative, incoming message: senders address.
extern char HDR_From2[SIZE_HEADER];

extern const char *HDR_Flash;	// Msg file input.
extern char HDR_Flash2[SIZE_HEADER];

extern const char *HDR_Provider;	// Msg file input.
extern char HDR_Provider2[SIZE_HEADER];

extern const char *HDR_Queue;	// Msg file input.
extern char HDR_Queue2[SIZE_HEADER];

extern const char *HDR_Binary;	// Msg file input (sets alphabet to 1 or 0).
extern char HDR_Binary2[SIZE_HEADER];

extern const char *HDR_Report;	// Msg file input. Incoming message: report was asked yes/no.
extern char HDR_Report2[SIZE_HEADER];

extern const char *HDR_Autosplit;	// Msg file input.
extern char HDR_Autosplit2[SIZE_HEADER];

extern const char *HDR_Validity;	// Msg file input.
extern char HDR_Validity2[SIZE_HEADER];

extern const char *HDR_Voicecall;	// Msg file input.
extern char HDR_Voicecall2[SIZE_HEADER];

extern const char *HDR_Replace;	// Msg file input. Incoming message: exists with code if replace
extern char HDR_Replace2[SIZE_HEADER];			// code was defined.

extern const char *HDR_Alphabet;	// Msg file input. Incoming message.
extern char HDR_Alphabet2[SIZE_HEADER];

extern const char *HDR_Include;	// Msg file input.
extern char HDR_Include2[SIZE_HEADER];

extern const char *HDR_Macro;	// Msg file input.
extern char HDR_Macro2[SIZE_HEADER];

extern const char *HDR_Hex;		// Msg file input.
extern char HDR_Hex2[SIZE_HEADER];

extern const char *HDR_SMSC;	// Msg file input: smsc number.
extern char HDR_SMSC2[SIZE_HEADER];

extern const char *HDR_Priority;	// Msg file input.
extern char HDR_Priority2[SIZE_HEADER];

extern const char *HDR_System_message; // Msg file input.
extern char HDR_System_message2[SIZE_HEADER];

extern const char *HDR_Smsd_debug;	// For debugging purposes
extern char HDR_Smsd_debug2[SIZE_HEADER];

extern const char *HDR_UDHDATA;	// Msg file input. Incoming message.
extern const char *HDR_UDHDUMP;	// Msg file input (for backward compatibility).
extern const char *HDR_UDH;		// Msg file input. Incoming binary message: "yes" / "no".

extern const char *HDR_Sent;	// Outgoing timestamp, incoming: senders date & time (from PDU).
extern char HDR_Sent2[SIZE_HEADER];

extern const char *HDR_Modem;	// Sent message, device name (=modemname). After >= 3.1.4 also incoming message.
extern char HDR_Modem2[SIZE_HEADER];

extern const char *HDR_Number;	// 3.1.4: Sent message, incoming message, SIM card's telephone number.
extern char HDR_Number2[SIZE_HEADER];

extern const char *HDR_FromTOA;	// Incoming message: senders Type Of Address.
extern char HDR_FromTOA2[SIZE_HEADER];

extern const char *HDR_FromSMSC;	// Incoming message: senders SMSC
extern char HDR_FromSMSC2[SIZE_HEADER];

extern const char *HDR_Name;	// Incoming message: name from the modem response (???).
extern char HDR_Name2[SIZE_HEADER];

extern const char *HDR_Received;	// Incoming message timestamp.
extern char HDR_Received2[SIZE_HEADER];

extern const char *HDR_Subject;	// Incoming message, modemname.
extern char HDR_Subject2[SIZE_HEADER];

extern const char *HDR_UDHType;	// Incoming message, type(s) of content of UDH if present.
extern char HDR_UDHType2[SIZE_HEADER];

extern const char *HDR_Length;	// Incoming message, text/data length. With Unicode: number of Unicode
extern char HDR_Length2[SIZE_HEADER];			// characters. With GSM/ISO: nr of chars, may differ if stored as UTF-8.

extern const char *HDR_FailReason;	// Failed outgoing message, error text.
extern char HDR_FailReason2[SIZE_HEADER];

extern const char *HDR_Failed;	// Failed outgoing message, timestamp.
extern char HDR_Failed2[SIZE_HEADER];

extern const char *HDR_Identity;	// Incoming / Sent(or failed), exists with code if IMSI request
extern char HDR_Identity2[SIZE_HEADER];			// supported.

extern const char *HDR_MessageId;	// Sent (successfully) message. There is fixed "message id" and
extern char HDR_MessageId2[SIZE_HEADER];			// "status" titled inside the body of status report.

extern const char *HDR_OriginalFilename;	// Stored when moving file from outgoing directory and
extern char HDR_OriginalFilename2[SIZE_HEADER];			// unique filenames are used in the spooler.

extern const char *HDR_CallType;	// Incoming message from phonebook.
extern char HDR_CallType2[SIZE_HEADER];

extern const char *HDR_missed;	// For incoming call type.
extern char HDR_missed2[SIZE_HEADER];

extern const char *HDR_missed_text;	// For incoming call, message body.
extern char HDR_missed_text2[SIZE_HEADER];

extern const char *HDR_Result;	// For voice call, result string from a modem
extern char HDR_Result2[SIZE_HEADER];

extern const char *HDR_Incomplete;	// For purged message files.
extern char HDR_Incomplete2[SIZE_HEADER];

extern char *EXEC_EVENTHANDLER;
extern char *EXEC_RR_MODEM;
extern char *EXEC_RR_POST_MODEM;
extern char *EXEC_RR_MAINPROCESS;
extern char *EXEC_CHECKHANDLER;

#endif
