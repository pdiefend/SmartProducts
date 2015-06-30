#include "c_types.h"
#include "ip_addr.h"
#include "ets_sys.h"
#include "espconn.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "ssl/cert.h"
#include "ssl/private_key.h"
#include "jsonparse.h"
#include "jsontree.h"
#include "ws2812.h"

//#include "cJSON.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
#define CLIENT_SSL_ENABLE

// Network variables
static esp_tcp global_tcp;
static struct espconn global_tcp_connect;
static uint8_t tcp_conn_ok = FALSE;

// OS variables
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static volatile os_timer_t updateTimer;
static volatile os_timer_t networkTimer;

// User variables
//ip_addr_t esp_server_ip;

// Forward Declarations
int atoi(char* s);
static void user_procTask(os_event_t *events);
void updateTimerISR(void *arg);
void user_init();
void networkInit();
void network_check_ip(void);
static void tcpNetworkRecvCb(void *arg, char *data, unsigned short len);
static void tcpNetworkConnectedCb(void *arg);
static void tcpNetworkReconCb(void *arg, sint8 err);
static void tcpNetworkDisconCb(void *arg);
static void init_tcp_conn(void);
//void dns_found_CB(const char *name, ip_addr_t *ipaddr, void *arg);


void updateTimerISR(void *arg) {
    // Start a new connection
    //setWS2812color(255, 0, 0);
    espconn_secure_connect(&global_tcp_connect);
/*
    uint8_t data[REQUEST_LEN] = REQUEST_URL;
    
    // Send the data
    espconn_secure_sent(&global_tcp_connect, data, REQUEST_LEN);
    os_printf(data);
    os_printf("\n");
*/
    // disarm the timer for now since I'm mostly doing one-shot tests
    // os_timer_disarm(&updateTimer);
}

/* Do nothing function */
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(1000);
}


void ICACHE_FLASH_ATTR user_rf_pre_init() {
    system_phy_set_rfoption(1);
}

/* User Init, code execution starts here from OS */
void ICACHE_FLASH_ATTR user_init() {
    uart_init(BIT_RATE_115200, BIT_RATE_115200);                    // Init UART @ 115200 bps    
    os_printf("\r\n\r\nBegin");
    
    /*
    uint8_t data[RESPONSE_LEN] = RESPONSE;
    struct jsonparse_state state;
    jsonparse_setup(&state, data, RESPONSE_LEN);
    
    char buffer[50];
    jsonparse_copy_value(&state, buffer, 50);
    os_printf(buffer);
    jsonparse_next(&state);
    jsonparse_copy_value(&state, buffer, 50);
    os_printf(buffer);
    */
    
    
    
    
    ETS_GPIO_INTR_DISABLE();                                        // Disable Interrupts
    gpio_init();                                                    // Enable GPIOS
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    ETS_GPIO_INTR_ENABLE();                                         // Enable interrupts

    //Setup timer
    os_timer_disarm(&updateTimer);
    os_timer_setfn(&updateTimer, (os_timer_func_t *)updateTimerISR, NULL);
    os_timer_arm(&updateTimer, 15000, 1); 
    
    networkInit();
    char ssid[32] = SSID;
    char password[64] = PASS;
    struct station_config stationConf;
    wifi_set_opmode(0x1);               // Client-only mode
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);
    wifi_station_set_config(&stationConf);
    //wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);     // Set GPIO2 as WiFi status LED

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");
    //setWS2812color(0, 0, 255);
    
    /*
    gpio_output_set(0, BIT2, BIT2, 1);
    gpio_output_set(0, BIT2, BIT2, 0);
    gpio_output_set(0, BIT2, BIT2, 1);
    gpio_output_set(0, BIT2, BIT2, 0);
    gpio_output_set(0, BIT2, BIT2, 1);
    gpio_output_set(0, BIT2, BIT2, 0);
    volatile int i = 0;
    while(i < 10)
        i++;
    gpio_output_set(0, BIT2, BIT2, 1);
    gpio_output_set(0, BIT2, BIT2, 0);
    */
}

/* network init function */
void ICACHE_FLASH_ATTR networkInit() {       
    os_timer_disarm(&networkTimer);
    os_timer_setfn(&networkTimer, (os_timer_func_t *)network_check_ip, NULL);
    os_timer_arm(&networkTimer, 1000, 0);
}

/* Check to see if we were issued an IP number which means successfull connected */
void ICACHE_FLASH_ATTR network_check_ip(void) {
    struct ip_info ipconfig;

    os_timer_disarm(&networkTimer);                // Disarm timer
    wifi_get_ip_info(STATION_IF, &ipconfig);        // Get Wifi info

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
        init_tcp_conn();
    }
    else {
        os_printf("Waiting for IP...\n\r");
        os_timer_setfn(&networkTimer, (os_timer_func_t *)network_check_ip, NULL);
        os_timer_arm(&networkTimer, 1000, 0);
    }         
}

/* tcp Data received callback. Print the data for diagnosic purposes. A Successful
** run returns the number datapoint that was just received. */    
static void ICACHE_FLASH_ATTR tcpNetworkRecvCb(void *arg, char *data, unsigned short len) {
    struct espconn *tcpconn=(struct espconn *)arg;
    //setWS2812color(0, 255, 0);
    os_printf("Recv: %d \n\r", len);
    os_printf(data); // <=======================================================================================
    os_printf("\n");
    
    char* ptr = strstr(data, "current_power"); // find the 'c' in substring current_power
    int idx = ptr-data;
    //espconn_secure_disconnect(&global_tcp_connect);
    // os_printf("Idx: %d\n", idx);
    idx += 15; // offset for <current_power":> 
    // Simple test to prove we found it. Need to actually parse integer from this
    // os_printf("Current Power (char): %c%c%c%c \r\n", data[idx], data[idx+1], data[idx+2], data[idx+3]);
    int power = atoi(&data[idx]);
    os_printf("Current Power (int): %d \r\n", power);
}

/* tcp Successfully connected Call back */
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg) {    
    struct espconn *tcpconn=(struct espconn *)arg;
    
    // register the call back function for when data is received
    espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);    

    os_printf("TCP connected\n\r"); 
    tcp_conn_ok = TRUE;
    
    uint8_t data[REQUEST_LEN] = REQUEST_URL;
    
    // Send the data
    espconn_secure_sent(&global_tcp_connect, data, REQUEST_LEN);
    os_printf(data);
    os_printf("\n");
}


/* Callback Function for when the network was reconnected, re-init stuff */
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err) {
    //os_printf("TCP reconnect\n\r");
    tcp_conn_ok = FALSE;
    // networkInit();
}

/* Callback Function for when the network was disconnected, re-init stuff */
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg) {
    os_printf("TCP disconnect\n\r");
    tcp_conn_ok = FALSE;
    // networkInit();
}

/* Initialize the TCP connection */
static void ICACHE_FLASH_ATTR init_tcp_conn(void) {
    //espconn_gethostbyname(&global_tcp_connect, "api.enphase.com", &esp_server_ip, dns_found_CB);
    
    global_tcp_connect.type=ESPCONN_TCP;
    global_tcp_connect.state=ESPCONN_NONE;
    global_tcp_connect.proto.tcp=&global_tcp;
    global_tcp_connect.proto.tcp->local_port=espconn_port();

    // change so we go to DNS rather than hardcoded IP address
    // IP to send data to 198.101.171.58 (api.enphaseenergy.com)
    global_tcp_connect.proto.tcp->remote_ip[0]=198;
    global_tcp_connect.proto.tcp->remote_ip[1]=101;
    global_tcp_connect.proto.tcp->remote_ip[2]=171;
    global_tcp_connect.proto.tcp->remote_ip[3]=58;
    global_tcp_connect.proto.tcp->remote_port=443; //443 for sercure or 80 for non-secure

    // Register Callbacks
    espconn_regist_connectcb(&global_tcp_connect, tcpNetworkConnectedCb);
    espconn_regist_disconcb(&global_tcp_connect, tcpNetworkDisconCb);
    espconn_regist_reconcb(&global_tcp_connect, tcpNetworkReconCb);

    // Buffer for SSL certificate
    espconn_secure_set_size(ESPCONN_CLIENT, 7680);

    // Fire it up
    // espconn_secure_connect(&global_tcp_connect);
  
    
}
/*
void ICACHE_FLASH_ATTR dns_found_CB(const char *name, ip_addr_t *ipaddr, void *arg) {
    struct espconn *pespconn = (struct espconn *)arg;
    os_printf("dns_found_CB %d . %d . %d . %d/n",
    *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
    *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));
}
*/

/* atoi: convert s to integer */
/*from Kernighan and Ritchie "The C Programming Language, pg 43 */ 
int atoi(char* s){
    int i, n = 0;
    
    for (i = 0; s[i] >= '0' && s[i] <= '9'; i++)
        n = 10 * n + (s[i] - '0');
    return n;
}
