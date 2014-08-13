#ifndef READSMS_H
#define READSMS_H


/* =======================================================================
   Read the header of an SMS file
   ======================================================================= */

void readSMSheader(char* filename,    // Filename */
                   int recursion_level,
                   // output variables are:
                   char* to, 		      // destination number */
                   char* from, 		    // sender name or number */
                   int*  alphabet, 	  // -1=GSM 0=ISO 1=binary 2=UCS2 3=UTF8 4=unknown */
                   int* with_udh,  	  // UDH flag */
                   char* udh_data,  	// UDH data in hex dump format. Only used in alphabet<=0 */
                   char* queue, 	    // Name of Queue */
                   int*  flash, 	    // 1 if send as Flash SMS */
                   char* smsc, 		    // SMSC Number */
                   int*  report,  	  // 1 if request status report */
                   int*  split,  	    // 1 if request splitting */
                   int*  validity, 	  // requested validity period value */
                   int*  voicecall, 	// 1 if request voicecall */
                   int* hex,		      // 1 if binary message is presented as hexadecimal */
                   int *replace_msg,	// 1...7, 0=none */
                   char *macros,
                   int *system_msg,	  // 1 if sending as a system message. */
                   int *to_type);     // -1 = default, -2 = error, >= 0 = accepted value  */

/* =======================================================================
   Read the message text or binary data of an SMS file
   ======================================================================= */

void readSMStext(char* filename, // Filename */
                 int recursion_level,
                 int do_convert, // shall I convert from ISO to GSM? Do not try to convert binary data. */
                 // output variables are:
                 char* text,     // message text */
                 int* textlen,   // text length */
                 char *macros);
void readSMShex(char *filename,
                int recursion_level, 
                char *text,
                int *textlen,
                char *macros,
                char *errortext);

#endif
