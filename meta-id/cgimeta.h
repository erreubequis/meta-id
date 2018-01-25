#ifndef CGIMETA_H
#define CGIMETA_H

#include "httpd.h"

int cgiMetaUserPass(HttpdConnData *connData);
int cgiMetaAuth(HttpdConnData *connData);
int cgiMetaCheckAuth(HttpdConnData *connData);
int cgiMetaCheckAuthCgi(HttpdConnData *connData);
int cgiMetaHome(HttpdConnData *connData);
int cgiMetaDump(HttpdConnData *connData);
int cgiMetaWav(HttpdConnData *connData);
int cgiMetaGpio(HttpdConnData *connData);
int cgiMetaGetSignal(HttpdConnData *connData);
void cgiMetaInit();
int metaCheckHash (int32 hash);
int cgiMetaLogout(HttpdConnData *connData);
int cgiMetaGetSSID(HttpdConnData *connData);
//int meta_init_gpio();
int ICACHE_FLASH_ATTR cgiMetaSend(HttpdConnData *connData) ;
extern char* rst_codes[7];
extern char* flash_maps[7];
extern char MetaLimen[16];

#endif // CGIMETA_H
