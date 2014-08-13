#ifndef MACRO_H
#define MACRO_H

/* =======================================================================
   Macros can be used with ISO/UTF-8 written messages with cs_convert=yes
   and with binary messages written with hex=yes.
   A buffer should be 0-terminated and it cannot include 0x00 characters.
   ======================================================================= */

int extract_macros(char *buffer, int buffer_size, char *macros);

#endif
