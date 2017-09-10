#ifndef CGIMETA_H
#define CGIMETA_H

#include "httpd.h"

int cgiMetaWav(HttpdConnData *connData);
int cgiMetaGpio(HttpdConnData *connData);
void ICACHE_FLASH_ATTR cgiMetaInit();

extern char* rst_codes[7];
extern char* flash_maps[7];
extern char MetaLimen[16];

#endif // CGIMETA_H
