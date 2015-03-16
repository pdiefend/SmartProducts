/* Heavily Modified from a guy named Limpkin who has a blog at
 http://www.limpkin.fr/index.php?post/2014/12/07/First-Steps-with-the-ESP8266-03-Development-Board
*/

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

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1

// forward declarations
void led_timerfunc(void *arg);
static void ICACHE_FLASH_ATTR tcpNetworkRecvCb(void *arg, char *data, unsigned short len);
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg);
void ICACHE_FLASH_ATTR network_init();
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err);
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg);
static void ICACHE_FLASH_ATTR init_tcp_conn(void);
void ICACHE_FLASH_ATTR network_start(void);
void ICACHE_FLASH_ATTR network_check_ip(void);
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events);
void ICACHE_FLASH_ATTR user_init();

// RTOS Process Queue
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

// RTOS Timers
static volatile os_timer_t led_timer;
LOCAL os_timer_t network_timer;

// Global Variables
static esp_tcp global_tcp;                              // TCP connect var (see espconn.h)
static struct espconn global_tcp_connect;               // Connection struct (see espconn.h)
static uint8_t tcp_conn_ok = FALSE;                     // Bool to know if tcp connection set

// Timer Function to blink an LED and check if button was pressed
void led_timerfunc(void *arg) {   
    // Toggle GPIO2
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {    // If GPIO2 high
        gpio_output_set(0, BIT2, BIT2, 0);          // Set GPIO2 to LOW
    } else {
        gpio_output_set(BIT2, 0, BIT2, 0);          // Set GPIO2 to HIGH
    }

    // If button was pressed then download weather data
    if ((GPIO_INPUT_GET(0) == 0) && (tcp_conn_ok == TRUE)) {
        uint8_t data[75] = "GET /data/2.5/weather?id=5201470 HTTP/1.0\r\nHost: api.openweathermap.org\r\n\r\n";
        espconn_sent(&global_tcp_connect, data, 75);
    }
}

// If data was received from the TCP connection
static void ICACHE_FLASH_ATTR tcpNetworkRecvCb(void *arg, char *data, unsigned short len) {
    // create a tempory esp connection struct containing metadata from received data    
    struct espconn *tcpconn=(struct espconn *)arg;
    // print data receieved
    os_printf(data);
}

// TCP Successfully connected Call back
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg) {    
    /* create a connection struct with passed connection data including successfully
    created connection data */
    struct espconn *tcpconn=(struct espconn *)arg;
    /* register a call back function for when data is received */
    espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);
    
    os_printf("TCP connected\n\r"); 
    tcp_conn_ok = TRUE;
}

/* Initialize network connections and then attempt to connect to WiFi checking every
second until we were issued an ip address from the DHCP server. */
void ICACHE_FLASH_ATTR network_init() {       
    os_timer_disarm(&network_timer);
    os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
    os_timer_arm(&network_timer, 1000, 0); // Timer triggers once per second until we have
    // have a connection were it is retirggered until we have an ip. 
}

/* TCP Reconnected Callback. Called when the connection was reestablished 
When this is called is unclear.*/
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err) {
    os_printf("TCP reconnect\n\r");
    tcp_conn_ok = FALSE;
    /* Reset the network connections */
    network_init();
}

/* TODO what is this and the previous function doing */
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg) {
    os_printf("TCP disconnect\n\r");
    tcp_conn_ok = FALSE;
    network_init();
}

/* Initialize the TCP connection */
static void ICACHE_FLASH_ATTR init_tcp_conn(void) {
    global_tcp_connect.type=ESPCONN_TCP;                     // We want to make a TCP connection
    global_tcp_connect.state=ESPCONN_NONE;                   // Set default state to none
    global_tcp_connect.proto.tcp=&global_tcp;                // Give a pointer to our global TCP struct
    global_tcp_connect.proto.tcp->local_port=espconn_port(); // Ask a free local port to the API
    
    // 188.226.224.148:80, api.openweathermap.org    
    global_tcp_connect.proto.tcp->remote_ip[0]=188;      // Remote IP
    global_tcp_connect.proto.tcp->remote_ip[1]=226;      // Remote IP
    global_tcp_connect.proto.tcp->remote_ip[2]=224;      // Remote IP
    global_tcp_connect.proto.tcp->remote_ip[3]=148;      // Remote IP
    global_tcp_connect.proto.tcp->remote_port = 80;      // Set remote port, default web server port

    espconn_regist_connectcb(&global_tcp_connect, tcpNetworkConnectedCb);   // Register connect callback
    espconn_regist_disconcb(&global_tcp_connect, tcpNetworkDisconCb);       // Register disconnect callback
    espconn_regist_reconcb(&global_tcp_connect, tcpNetworkReconCb);         // Register reconnection function
    espconn_connect(&global_tcp_connect);                                   // Start connection
}


/* Start network connections */
void ICACHE_FLASH_ATTR network_start(void) {   
    init_tcp_conn();            // Init tcp connection
}

/* Check to see if we were issued an IP number. See comments in Server program */
void ICACHE_FLASH_ATTR network_check_ip(void) {
    struct ip_info ipconfig;

    os_timer_disarm(&network_timer);                // Disarm timer
    wifi_get_ip_info(STATION_IF, &ipconfig);        // Get Wifi info

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
        network_start();                            // Everything in order
    } else {
        os_printf("Waiting for IP...\n\r");
        os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
        os_timer_arm(&network_timer, 1000, 0);
    }         
}

/* Do nothing function */
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(100);
}

/* Init function */
void ICACHE_FLASH_ATTR user_init() {    
    uart_init(BIT_RATE_57600, BIT_RATE_57600);    // Init UART @ 57600 bps    
                                                  
    ETS_GPIO_INTR_DISABLE();                      // Disable gpio interrupts
    gpio_init();                                  // Initialize the GPIO subsystem.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);    // Set GPIO2 function
    gpio_output_set(0, BIT2, BIT2, 0);                      // Set GPIO2 low output
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO0);    // Set GPIO0 function
    gpio_output_set(0, 0, 0, BIT0);                        // Set GPIO0 as input
    
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_MTCK_U);                 // Disable pulldown
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);                   // Enable pullup
    
    os_timer_disarm(&led_timer);                            // Disarm led timer
    os_timer_setfn(&led_timer, (os_timer_func_t *)led_timerfunc, NULL); // Setup led timer
    os_timer_arm(&led_timer, 500, 1);                       // Arm led timer, 0.5sec, repeat
    
    network_init();                                         // Init network timer
    
    char ssid[32] = SSID;                                   // Wifi SSID
    char password[64] = SSID_PASSWORD;                      // Wifi Password
    struct station_config stationConf;                      // Station conf struct    
    wifi_set_opmode(0x1);                                   // Set station mode
    os_memcpy(&stationConf.ssid, ssid, 32);                 // Set settings
    os_memcpy(&stationConf.password, password, 64);         // Set settings
    wifi_station_set_config(&stationConf);                  // Set wifi conf
    //wifi_status_led_install(13, PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);  // Wifi LED status  
    
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");                                  // Startup done
}

/* 
First connect to the WiFi network
Set to single connection mode
Start TCP connection to DST_IP port 80
Send the following data:
    cmd = "GET /data/2.5/weather?id=";
    cmd += LOCATIONID;
    cmd += " HTTP/1.0\r\nHost: api.openweathermap.org\r\n\r\n";
    maybe make that at static string of known length
JSON will get returned, maybe just dump the data to the terminal until I know what to look for
    look for "\"main\":{" also according to previous program then stop at }
set this up on a timer for once every so many seconds
*/

