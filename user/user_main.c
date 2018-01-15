/*#include <esp8266.h>
#include <config.h>

// initialize the custom stuff that goes beyond meta-id
void app_init() {

}
#if 0 // working udp server
*/
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "gpio.h"
//#include "at_custom.h"
#include "osapi.h"
#define DBG(format, ...) do { os_printf(format "\n", ## __VA_ARGS__); } while(0)

/**
 * Minimal example: Setup wifi for station mode, setup connection, wait for an IP.
 * If an IP has been assigned (we poll for that every 3s), start a udp socket. No remote
 * IP set, only local port set.
 * Expected: Broadcast UDP works.
 * Current behaviour: Only unicast traffic is accepted, no broadcast packets.
 */

static struct espconn* pUdpServer;
static os_timer_t timer_check_connection;

static void ICACHE_FLASH_ATTR network_received(void *arg, char *data, unsigned short len) ;
static void ICACHE_FLASH_ATTR network_udp_start(void) ;
static void ICACHE_FLASH_ATTR timeout_func(void *arg);
static void ICACHE_FLASH_ATTR init_check_timer();
//extern void uart_init(int uart0_br, int uart1_br);

void app_init(void)
{  
//    uart_init(115200, 115200);
//    at_init();

    //Set station mode
/*    wifi_set_opmode( 0x1 );

    //Set ap settings
    char ssid[32] = "AP_NAME";
    char password[64] = "AP_PWD";
    struct station_config stationConf;
    os_memcpy(&stationConf.ssid, ssid, sizeof(ssid));
    os_memcpy(&stationConf.password, password, sizeof(password));
    wifi_station_set_config(&stationConf);*/
    init_check_timer();
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
  
    network_udp_start();
}

static void ICACHE_FLASH_ATTR network_udp_start(void) 
{   
	sint16 connstatus;
    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
    pUdpServer->type=ESPCONN_UDP;
    pUdpServer->state=ESPCONN_NONE;
    pUdpServer->proto.udp= (esp_udp *)os_zalloc(sizeof(esp_udp));
    pUdpServer->proto.udp->local_port=53;                          // Set local port to 2222
    //pUdpServer->proto.udp->remote_port=3338;                         // Set remote port
    connstatus=espconn_create(pUdpServer) ;
    if(connstatus== 0)
    {
        espconn_regist_recvcb(pUdpServer, network_received);
        DBG("UDP OK");
    }
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
}

static void ICACHE_FLASH_ATTR network_received(void *arg, char *data, unsigned short len) 
{
    // print ACK on UART
    DBG("UDP received");

    // send ACK to sender via udp
    struct espconn *udpconn=(struct espconn *)arg;
  remot_info *premot = NULL;
  if(espconn_get_connection_info(udpconn,&premot,0)==ESPCONN_OK){
  udpconn->proto.udp->remote_port = premot->remote_port;
  udpconn->proto.udp->remote_ip[0] = premot->remote_ip[0];
  udpconn->proto.udp->remote_ip[1] = premot->remote_ip[1];
  udpconn->proto.udp->remote_ip[2] = premot->remote_ip[2];
  udpconn->proto.udp->remote_ip[3] = premot->remote_ip[3];
    static const char msg[] = "REC\n";
    espconn_sent(udpconn, (uint8 *)msg, sizeof(msg));
	DBG ("reply ok");
}
else{
	DBG ("reply ko");
	}
}
//#endif