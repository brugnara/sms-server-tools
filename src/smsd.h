#ifndef SMSD_H
#define SMSD_H

char *get_header_incoming(const char *header, char *header2);
int apply_filename_preview(char *filename, char *arg_text, int setting_length);
void send_admin_message(int *quick, int *errorcounter, char *text);
void sendsignal2devices(int signum);

#endif
