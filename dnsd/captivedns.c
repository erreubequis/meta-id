#include <esp8266.h>
#include "captivedns.h"

#define CDNS_DBG
#ifdef CDNS_DBG
#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)
#else
#define DBG(format, ...) do { } while(0)
#endif

/* arpa internet macros taken from https://github.com/esp8266/Arduino/blob/master/libraries/Ethernet/src/utility/util.h */
#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)


#define MAX_SENDBUFF_LEN 2600

// debug string to identify connection (ip address & port)
// a static string works because callbacks don't get interrupted...
static char connStr[24];


static void debugConn(void *arg, char *what) {
#if 1
  struct espconn *espconn = arg;
  esp_udp *udp = espconn->proto.udp;
  os_sprintf(connStr, "%d.%d.%d.%d:%d ",
    udp->remote_ip[0], udp->remote_ip[1], udp->remote_ip[2], udp->remote_ip[3],
    udp->remote_port);
  DBG("%s %s\n", connStr, what);
#else
  connStr[0] = 0;
#endif
}

void cdnsDowncaseAndRemoveWwwPrefix(char*domainName);
char cdnsRequestIncludesOnlyOneQuestion(DNSHeader head);
void cdnsReplyWithIP(espconn *conn, char *data, unsigned short len);
void cdnsReplyWithCustomCode(espconn *conn, char *data, unsigned short len);
static os_timer_t timer_check_connection;
static void ICACHE_FLASH_ATTR timeout_func(void *arg);
static void ICACHE_FLASH_ATTR init_check_timer();
void cdnsStart();

//Listening connection data
static struct espconn cdnsConn;
static esp_udp cdnsUdp;
//static CaptiveDNS CDNS;

static char CDNSdomainName[64];
static unsigned char CDNSresolvedIP[4];

//static void ICACHE_FLASH_ATTR cdnsSentCb(void *arg);
static void ICACHE_FLASH_ATTR cdnsRecvCb(void *arg, char *data, unsigned short len);

void hd(char*data,unsigned short len){
	for(int i=0;i<len;i++){
		if(i>0){
		if(i%16==0)os_printf("\n");
		else if(i%8==0)os_printf("  ");
		else if(i%2==0)os_printf(" ");
		}
		os_printf("%02x",data[i]);
	}os_printf("\n");
}

void debugDNS(DNSHeader head){
	os_printf("ID : %04x\tRD : %d\tTC : %d\tAA :%d\n",head.ID,head.RD,head.TC,head.AA );
	os_printf("OPC: %d\tQR : %d\tRC : %d\tZ  :%d\tRA :%d\n",head.OPCode,head.QR,head.RCode,head.Z ,head.RA );
	os_printf("QDCount : %d\nANCount : %d\nNSCount : %d\nARCount :%d\n\n",
		ntohs(head.QDCount),ntohs(head.ANCount),ntohs(head.NSCount),ntohs(head.ARCount) );
	os_printf("onlyonequestion : %d\n",cdnsRequestIncludesOnlyOneQuestion(head) );

}
//Callback called when there's data available on a socket.
static void ICACHE_FLASH_ATTR cdnsRecvCb(void *arg, char *data, unsigned short len) {
  debugConn(arg, "cdnsRecvCb");
  struct espconn* conn = arg;
  remot_info *premot = NULL;
	hd(data,len);
  if(espconn_get_connection_info(conn,&premot,0)==ESPCONN_OK){
 /* if (conn == NULL) {
	  DBG("CDNS: aborted connection");
	  return; 
  }*/
  conn->proto.udp->remote_port = premot->remote_port;
  conn->proto.udp->remote_ip[0] = premot->remote_ip[0];
  conn->proto.udp->remote_ip[1] = premot->remote_ip[1];
  conn->proto.udp->remote_ip[2] = premot->remote_ip[2];
  conn->proto.udp->remote_ip[3] = premot->remote_ip[3];
	DNSHeader *phead;
	phead=(DNSHeader *)data;
	DBG("received packet:");
	debugDNS(*phead);
  //Send the response.
    if (phead->QR == DNS_QR_QUERY &&
        phead->OPCode == DNS_OPCODE_QUERY &&
        cdnsRequestIncludesOnlyOneQuestion(*phead) 
       ){
      cdnsReplyWithIP(conn, data,len);
    }
    else if (phead->QR == DNS_QR_QUERY){
      cdnsReplyWithCustomCode(conn, data,len);
    }
  }
}

static void ICACHE_FLASH_ATTR init_check_timer() {
    //Disarm timer
    os_timer_disarm(&timer_check_connection);

    //Setup and arm timer
    os_timer_setfn(&timer_check_connection, (os_timer_func_t *)timeout_func, 0);
    os_timer_arm(&timer_check_connection, 3000, 1);

    timeout_func(0);
}

static void ICACHE_FLASH_ATTR timeout_func(void *arg)
{
    // We execute this timer function as long as we do not have an IP assigned
    struct ip_info info;
    wifi_get_ip_info(SOFTAP_IF, &info);
    
    DBG("...");
    
    if (info.ip.addr == 0)
      return;

    // IP assigned, disarm timer
    os_timer_disarm(&timer_check_connection);
  
    cdnsStart();
}

void cdnsInit(const char* domainName, const uint16_t port){
	cdnsConn.type = ESPCONN_UDP;
	cdnsConn.state = ESPCONN_NONE;
	cdnsUdp.local_port = port;
	cdnsConn.proto.udp = &cdnsUdp;
	os_sprintf(CDNSdomainName, domainName);
	cdnsDowncaseAndRemoveWwwPrefix(CDNSdomainName);
	init_check_timer();
}

void cdnsStart(){
	sint16 connstatus;
	struct ip_info info;
	wifi_get_ip_info(SOFTAP_IF, &info);
	CDNSresolvedIP[0]= ip4_addr1_16(&info.ip.addr);
	CDNSresolvedIP[1]= ip4_addr2_16(&info.ip.addr);
	CDNSresolvedIP[2]= ip4_addr3_16(&info.ip.addr);
	CDNSresolvedIP[3]= ip4_addr4_16(&info.ip.addr);
	DBG("CDNS init, conn=%p\n", &cdnsConn);
	connstatus=espconn_create(&cdnsConn) ;
	if(connstatus== 0){
		espconn_regist_recvcb(&cdnsConn, cdnsRecvCb);
		DBG("CDNS server created\n");
	}
#ifdef CDNS_DBG
	else{
        DBG("UDP KO");
        switch(connstatus){
			case ESPCONN_ISCONN:
				DBG("isconn");
			break;
			case ESPCONN_MEM:
				DBG("mem");
			break;
			case ESPCONN_ARG:
				DBG("arg");
			break;
			default:
				DBG("unknown");
			break;
			}
	}
#endif
}
/*
void cdnsSetErrorReplyCode(CDNSConnData *conn,uint8 replyCode){
  conn->errorReplyCode = replyCode;
}

void cdnsSetTTL(CDNSConnData *conn, const uint32_t ttl){
  conn->ttl = htonl(ttl);
}
*/

void cdnsDowncaseAndRemoveWwwPrefix(char*domainName){
	int i;
//  domainName.toLowerCase();
i=0;
while(domainName[i]){
	if (domainName[i]>='A' && domainName[i]<='Z'){
		domainName[i]+='a'-'A';
	}
	i++;
}
//  domainName.replace("www.", "");
if(os_strncmp(domainName,"www.",4)==0){
i=0;
while(domainName[i+4]){
		domainName[i]=domainName[i+4];
}
	i++;
	}
	domainName[i]='\0';
}

char cdnsRequestIncludesOnlyOneQuestion(DNSHeader head)
{
  return ntohs(head.QDCount) == 1 &&
         head.ANCount == 0 &&
         head.NSCount == 0 && 1;
         //head.ARCount == 0;
}

void cdnsReplyWithIP(espconn *conn,  char* data, unsigned short len){
	char buffer[256];
	int ttl=htonl(60);
	int l=len;
	DNSHeader *phead;
	phead=(DNSHeader *)data;
  phead->QR = DNS_QR_RESPONSE;
  phead->ANCount = phead->QDCount;
	DBG("CDNS: reply with ip\n");
	debugDNS(*phead);
//  head.QDCount = head.QDCount; 
  //_dnsHeader->RA = 1;  
	os_memcpy(buffer,data,len);
//	l+=os_sprintf(buffer+l,"%d",len);
// 192 -> pointer ; 12 : offset at 0x00c ; 01: type A query ; 01 class IN
	l+=os_sprintf(buffer+l,"%c%c%c%c%c%c",192,12,0,1,0,1);
// ttl - fixed on 4 chars
	os_memcpy(buffer+l,(unsigned char*)&ttl,4);
	l+=4;
	l+=os_sprintf(buffer+l,"%c%c%c%c%c%c",0,4,CDNSresolvedIP[0],CDNSresolvedIP[1],CDNSresolvedIP[2],CDNSresolvedIP[3]);
    // tcp : sint8 status = espconn_sent(conn->conn, (uint8*)buffer, l);
	hd(buffer,l);
    sint16 status = espconn_sendto(conn,(uint8*)buffer, l);
    if (status != 0) {
      DBG("ERROR! CDNS: espconn_sent returned %d, trying to send %d\n",status, l);
    }
  }


void cdnsReplyWithCustomCode(espconn *conn, char *data, unsigned short len){
	DNSHeader *phead;
	phead=(DNSHeader *)data;
  phead->QR = DNS_QR_RESPONSE;
  phead->RCode = (unsigned char) NonExistentDomain;
  phead->QDCount = 0;
      DBG("CDNS: reply with error\n");
	debugDNS(*phead);
	
    uint8 status = espconn_sendto(conn, (unsigned char*)data, sizeof(DNSHeader));
    if (status != 0) {
      DBG("ERROR! CDNS: espconn_sent returned %d, trying to send %d \n",status, sizeof(DNSHeader));
    }
}

