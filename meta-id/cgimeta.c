/*
 * 
 * TODO - readme : 
 * 
 * https://github.com/jeelabs/esp-link/blob/master/README.md
 * https://github.com/Spritetm/libesphttpd
 * api & tech reference
 * 
 * */


#include <esp8266.h>
#include "cgiwifi.h"
#include "cgi.h"
#include "config.h"
#include "sntp.h"
#include "cgimqtt.h"
#include "cgiwifi.h"
#include "web-server.h"
#ifdef SYSLOG
#include "syslog.h"
#endif
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)

// todo : typedef MetaLimen
short MetaLimen[16]; // if -2 : OUTPUT LOW ; -1 : OUTPUT HIGH; 0: undef; >0: INPUT VALUE

void ICACHE_FLASH_ATTR cgiMetaInit() {
  for (int i=0; i<16; i++) {
	MetaLimen[i] = 0;
	}
}

int ICACHE_FLASH_ATTR cgiMetaGetSignal(HttpdConnData *connData) {
  char buff[1024];
  int len;
  if (connData->conn==NULL) return HTTPD_CGI_DONE;
  len = os_sprintf(buff, "{ \"signal\":%d}",	wifiSignalStrength(-1));
  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}


int ICACHE_FLASH_ATTR cgiMetaGetGpio(HttpdConnData *connData) {
  char buff[1024];
  int len;
  if (connData->conn==NULL) return HTTPD_CGI_DONE;
  len = os_sprintf(buff, "{ ");
  for (int i=0; i<15; i++) {
 	if (MetaLimen[i]>0 ){ 
		MetaLimen[i] = GPIO_INPUT_GET(GPIO_ID_PIN(i))+1;
	}
	len += os_sprintf(buff+len, "\"meta-gpio-%02d\":%d, ",i,MetaLimen[i]);
  }
 	if (MetaLimen[15]>0 ){ 
		MetaLimen[15] = GPIO_INPUT_GET(GPIO_ID_PIN(15))+1;
	}
	len += os_sprintf(buff+len, "\"meta-gpio-%02d\":%d ",15,MetaLimen[15]);
	len += os_sprintf(buff+len,"}");
  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;

}
	
int ICACHE_FLASH_ATTR cgiMetaSetGpio(HttpdConnData *connData) {
// set input/output high/output low

  char buff[1024];
  int len;
  int8_t ok = 0; // error indicator
  int8_t num;
  int8_t out;
  ok |= getInt8Arg(connData, "num", &num);
  ok |= getInt8Arg(connData, "v", &out);
	if (!ok){
		len = os_sprintf(buff, "invalid parameter");
		jsonHeader(connData, 200);
		httpdSend(connData, buff, len);
		return HTTPD_CGI_DONE;
	}
	if (num < 0 || num > 15){
		len = os_sprintf(buff, "ko: invalid number %d ",num);
		jsonHeader(connData, 200);
		httpdSend(connData, buff, len);
		return HTTPD_CGI_DONE;
	}
	if (out < -2 || out > 2){
		len = os_sprintf(buff, "ko: invalid value %d ",out);
		jsonHeader(connData, 200);
		httpdSend(connData, buff, len);
		return HTTPD_CGI_DONE;
	}
  DBG("META: setting gpio |%d| to |%d| - old : %d\n",num,out, MetaLimen[num]);
	if(MetaLimen[num]==0){
		switch(num){
			case 0:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO0); break;
			case 1:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO1); break;
			case 2:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO2); break;
			case 3:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO3); break;
			case 4:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO4); break;
			case 5:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO5); break;
//			case 6:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO6); break;
//			case 7:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO7); break;
//			case 8:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO8); break;
			case 9:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO9); break;
			case 10:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO10); break;
//			case 11:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO11); break;
			case 12:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO12); break;
			case 13:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO13); break;
			case 14:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO14); break;
			case 15:PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO15); break;
			default:
				len = os_sprintf(buff, "ko: unhandled number %d ",num);
				jsonHeader(connData, 200);
				httpdSend(connData, buff, len);
				DBG("META: KO for |%d| to |%d| - old : %d\n",num,out, MetaLimen[num]);
				return HTTPD_CGI_DONE;
		}
	}

  len = os_sprintf(buff, "%d",ok);
  if(out == -1){
	GPIO_OUTPUT_SET(GPIO_ID_PIN(num), 1); 
	PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U); 
	MetaLimen[num] = -1;
	  }
  if(out == -2){
	GPIO_OUTPUT_SET(GPIO_ID_PIN(num), 0); 
	PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U); 
	  }
  if(out == 1){
	GPIO_DIS_OUTPUT(GPIO_ID_PIN(num)); 
	  }
   MetaLimen[num]=out;

  DBG("META: ok for |%d| to |%d| - old : %d\n",num,out, MetaLimen[num]);
  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;

}



int ICACHE_FLASH_ATTR cgiMetaGpio(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (connData->requestType == HTTPD_METHOD_GET) {
    return cgiMetaGetGpio(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return cgiMetaSetGpio(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}


int genSound(int* pos, char header[],int len){
    int i;
    int j;
    DBG("META : generating sound %d\n",*pos);
	if(*pos > 1024) 
		return 0;
	for (i = 0, j = *pos  * 1024; i < len; i += 2, j++ ){
        short datum1 = 450 * ((j >> 9 | j >> 7 | j >> 2) % 128);
        short datum2 = 450 * ((j >> 11 | j >> 8 | j >> 3) % 128);
        header[i]     = datum1; // One channel.
        header[i + 1] = datum2; // Another channel.
    }
    *pos=*pos+1;
	return 1024;
}

int genHeader(char header[],int len){
/******************************
*  Magic file format strings. *
******************************/
const char fChunkID[]     = {'R', 'I', 'F', 'F'};
const char fFormat[]      = {'W', 'A', 'V', 'E'};
const char fSubchunk1ID[] = {'f', 'm', 't', ' '};
const char fSubchunk2ID[] = {'d', 'a', 't', 'a'};

/********************************
* WriteWavePCM() configuration: *
* - 2 channels,                 *
* - frequency 44100 Hz.         *
********************************/
const unsigned short N_CHANNELS = 2;
const unsigned int SAMPLE_RATE = 44100;
const unsigned short BITS_PER_BYTE = 8;
const unsigned int N_SAMPLE_PAIRS = 1048576;

    const static unsigned int fSubchunk1Size = 16;
    const static unsigned short fAudioFormat = 1;
    const static unsigned short fBitsPerSample = 16;

    unsigned int fByteRate = SAMPLE_RATE * N_CHANNELS *
                             fBitsPerSample / BITS_PER_BYTE;

    unsigned short fBlockAlign = N_CHANNELS * fBitsPerSample / BITS_PER_BYTE;
    unsigned int fSubchunk2Size;
    unsigned int fChunkSize;

    //char header[46];
	memset(header,0,46);

    fSubchunk2Size =  N_SAMPLE_PAIRS * N_CHANNELS *2*sizeof(short);
    fChunkSize = 46 + fSubchunk2Size;
    DBG("META : generating header 1\n");
	memcpy(header,&fChunkID,4);
	memcpy(header+4,&fChunkSize,4);
	memcpy(header+8,&fFormat,4);
    DBG("META : generating header 2\n");

	memcpy(header+12,&fSubchunk1ID,4);
	memcpy(header+16,&fSubchunk1Size,4);
	memcpy(header+20,&fAudioFormat,2);
	memcpy(header+22,&N_CHANNELS,2);
	memcpy(header+24,&SAMPLE_RATE,4);
	memcpy(header+28,&fByteRate,4);
	memcpy(header+32,&fBlockAlign,2);
	memcpy(header+34,&fBitsPerSample,2);

    DBG("META : generating header 3\n");
	memcpy(header+36,&fSubchunk2ID,4);
	memcpy(header+40,&fSubchunk2Size,4);
	DBG("META : genheader done\n");
	return 44;
}


int ICACHE_FLASH_ATTR cgiMetaWav(HttpdConnData *connData) {
	int *pos=connData->cgiData;
	char buff[1024];
	int len ;
	//os_printf("cgiEspFsHook conn=%p conn->conn=%p file=%p\n", connData, connData->conn, file);

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (pos==NULL) {
		//First call to this cgi
		pos=os_malloc(sizeof(int));
		if (pos==NULL) {
			return HTTPD_CGI_NOTFOUND;
		}
		*pos=-1;
		connData->cgiData=pos;
		httpdStartResponse(connData, 200);
//		noCacheHeaders(connData, 200);
		httpdHeader(connData, "Content-Type", "audio/x-wav");
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}
	if (*pos==-1) {
		len=genHeader(buff, 1024);
		if (len>0) {
			espconn_sent(connData->conn, (uint8 *)buff, len);
			*pos=0;
			return HTTPD_CGI_MORE;
		}
		else{
			return HTTPD_CGI_NOTFOUND;
		}
	}
	len=genSound(pos, buff, 1024);
	if (len>0) espconn_sent(connData->conn, (uint8 *)buff, len);
	if (len!=1024) {
		//We're done.
		os_free(pos);
		return HTTPD_CGI_DONE;
	} 
		//Ok, till next time.
		return HTTPD_CGI_MORE;
}

#if 0
/*
from https://codereview.stackexchange.com/questions/105272/writing-computer-generated-music-to-a-wav-file-in-c
* 
 bool WriteWavePCM(short* sound, size_t pairAmount, char* fileName){

 * */
/******************************
*  Magic file format strings. *
******************************/
const char fChunkID[]     = {'R', 'I', 'F', 'F'};
const char fFormat[]      = {'W', 'A', 'V', 'E'};
const char fSubchunk1ID[] = {'f', 'm', 't', ' '};
const char fSubchunk2ID[] = {'d', 'a', 't', 'a'};

/********************************
* WriteWavePCM() configuration: *
* - 2 channels,                 *
* - frequency 44100 Hz.         *
********************************/
const unsigned short N_CHANNELS = 1;
const unsigned int SAMPLE_RATE = 44100;
const unsigned short BITS_PER_BYTE = 8;
const unsigned int N_SAMPLE_PAIRS = 1024; /*1048576;*/

    const static unsigned int fSubchunk1Size = 16;
    const static unsigned short fAudioFormat = 1;
    const static unsigned short fBitsPerSample = 16;

    unsigned int fByteRate = SAMPLE_RATE * N_CHANNELS *
                             fBitsPerSample / BITS_PER_BYTE;

    unsigned short fBlockAlign = N_CHANNELS * fBitsPerSample / BITS_PER_BYTE;
    unsigned int fSubchunk2Size;
    unsigned int fChunkSize;

    char header[46+N_SAMPLE_PAIRS * N_CHANNELS*2*sizeof(short)];
	memset(header,0,46+N_SAMPLE_PAIRS * N_CHANNELS*2*sizeof(short));
	short *sound;
    int i;
    int j;

    DBG("META : generating");
	sound=(short*)(header+46);
    for (i = 0, j = 0; i < N_SAMPLE_PAIRS * N_CHANNELS; i += 2, j++ )
    {
        short datum1 = 450 * ((j >> 9 | j >> 7 | j >> 2) % 128);
        short datum2 = 450 * ((j >> 11 | j >> 8 | j >> 3) % 128);

        sound[i]     = datum1; // One channel.
        sound[i + 1] = datum2; // Another channel.
    }

    fSubchunk2Size =  N_SAMPLE_PAIRS * N_CHANNELS *2*sizeof(short);
    fChunkSize = 46 + fSubchunk2Size;
    DBG("META : generating header 1\n");
	memcpy(header,&fChunkID,4);
	memcpy(header+4,&fChunkSize,4);
	memcpy(header+8,&fFormat,4);
    DBG("META : generating header 2\n");

	memcpy(header+12,&fSubchunk1ID,4);
	memcpy(header+16,&fSubchunk1Size,4);
	memcpy(header+20,&fAudioFormat,2);
	memcpy(header+22,&N_CHANNELS,2);
	memcpy(header+24,&SAMPLE_RATE,4);
	memcpy(header+28,&fByteRate,4);
	memcpy(header+32,&fBlockAlign,2);
	memcpy(header+34,&fBitsPerSample,2);

    DBG("META : generating header 3\n");
	memcpy(header+36,&fSubchunk2ID,4);
	memcpy(header+40,&fSubchunk2Size,4);
	DBG("META : sending header\n");
   noCacheHeaders(connData, 200);
  httpdHeader(connData, "Content-Type", "audio/x-wav");
//  httpdHeader(connData, "Content-Length", fChunkSize);
  httpdEndHeaders(connData);
DBG("META : sending data\n");
   
    httpdSend(connData, header, fChunkSize);
    httpdSend(connData, header+44, -1);
    DBG("META : done");

/*
    // Writing the RIFF header:
    fwrite(&fChunkID, 1, sizeof(fChunkID),      fout);
    fwrite(&fChunkSize,  sizeof(fChunkSize), 1, fout);
    fwrite(&fFormat, 1,  sizeof(fFormat),       fout);

    // "fmt" chunk:
    fwrite(&fSubchunk1ID, 1, sizeof(fSubchunk1ID),      fout);
    fwrite(&fSubchunk1Size,  sizeof(fSubchunk1Size), 1, fout);
    fwrite(&fAudioFormat,    sizeof(fAudioFormat),   1, fout);
    fwrite(&N_CHANNELS,      sizeof(N_CHANNELS),     1, fout);
    fwrite(&SAMPLE_RATE,     sizeof(SAMPLE_RATE),    1, fout);
    fwrite(&fByteRate,       sizeof(fByteRate),      1, fout);
    fwrite(&fBlockAlign,     sizeof(fBlockAlign),    1, fout);
    fwrite(&fBitsPerSample,  sizeof(fBitsPerSample), 1, fout);

    // "data" chunk: 
    fwrite(&fSubchunk2ID, 1, sizeof(fSubchunk2ID),      fout);
    fwrite(&fSubchunk2Size,  sizeof(fSubchunk2Size), 1, fout);

    // sound data: 
    fwrite(sound, sizeof(short),  N_SAMPLE_PAIRS * N_CHANNELS, fout);
*/
/*
  else {
   noCacheHeaders(connData, 200);
    httpdEndHeaders(connData);
  httpdHeader(connData, "Content-Type", "application/json");
    httpdSend(connData, "Failed to get sound", -1);
  }*/
  return HTTPD_CGI_DONE;
//    return true;
}

#endif
