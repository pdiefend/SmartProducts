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

// Network variables
static esp_tcp global_tcp;
static struct espconn global_tcp_connect;
static uint8_t tcp_conn_ok = FALSE;

// OS variables
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static volatile os_timer_t some_timer;
static volatile os_timer_t network_timer;

// User variables
static uint16_t counter = 0;

// Forward Declarations
static void user_procTask(os_event_t *events);
void some_timerfunc(void *arg);
void buttonISR(int8_t key);
void user_init();
void network_init();
void network_check_ip(void);
static void tcpNetworkRecvCb(void *arg, char *data, unsigned short len);
static void tcpNetworkConnectedCb(void *arg);
static void tcpNetworkReconCb(void *arg, sint8 err);
static void tcpNetworkDisconCb(void *arg);
static void init_tcp_conn(void);

void some_timerfunc(void *arg) {
    ETS_GPIO_INTR_DISABLE();                        // Disable gpio interrupts
    uint16_t drop_copy = counter;
    counter = 0;
    ETS_GPIO_INTR_ENABLE();                         // Enable gpio interrupts

    os_printf("Button Pressed %d Times\n", drop_copy);

    uint8_t data[100] = "GET /update?api_key=4U8GOF201NG3X7WM&field1=0000\r\n\r\n"; //at 43, 0 offset

    if(drop_copy > 9999) drop_copy = 9999;

    data[44] = (drop_copy / 1000) + 48;
    drop_copy = drop_copy % 1000;
    data[45] = (drop_copy / 100) + 48;
    drop_copy = drop_copy % 100;
    data[46] = (drop_copy / 10) + 48;
    drop_copy = drop_copy % 10;
    data[47] =  drop_copy + 48;
 
    //uint8_t data[100] = "GET /update?api_key=4U8GOF201NG3X7WM&field1=10 HTTP/1.0\r\nHost: http://api.thingspeak.com\r\n\r\n";
    //uint8_t data[20] = "https://api.thingspeak.com/update?api_key=4U8GOF201NG3X7WM%20&field1=8"
    
    espconn_sent(&global_tcp_connect, data, 100);
    os_printf(data);
    
}

//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(1000);
}

void buttonISR(int8_t key) {
    counter++;
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));
}


//Init function 
void ICACHE_FLASH_ATTR user_init() {
    wifi_station_disconnect();
    uart_init(BIT_RATE_57600, BIT_RATE_57600);    // Init UART @ 57600 bps    
    
    ETS_GPIO_INTR_DISABLE();
    gpio_init();

    ETS_GPIO_INTR_ATTACH(buttonISR, 0);          // GPIO0 interrupt handler
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO0);    // Set GPIO0 function
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(0));              // Set GPIO0 as input
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));      // Clear GPIO0 status
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_POSEDGE);            // Interrupt on posedge
    ETS_GPIO_INTR_ENABLE();                                 // Enable gpio interrupts

    //Setup timer
    os_timer_disarm(&some_timer);
    os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
    os_timer_arm(&some_timer, 30000, 1);
    
    network_init();                                         // Init network timer
    
    char ssid[32] = SSID;                        // Wifi SSID
    char password[64] = PASS;                      // Wifi Password
    struct station_config stationConf;                      // Station conf struct    
    wifi_set_opmode(0x1);                                   // Set station mode
    os_memcpy(&stationConf.ssid, ssid, 32);                 // Set settings
    os_memcpy(&stationConf.password, password, 64);         // Set settings
    wifi_station_set_config(&stationConf);                  // Set wifi conf
    wifi_status_led_install(2, PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO2);  // Wifi LED status

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");                                  // Startup done
}

// network init function
/* See comments in other program */
void ICACHE_FLASH_ATTR network_init() {       
    os_timer_disarm(&network_timer);
    os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
    os_timer_arm(&network_timer, 1000, 0);
}


/* Check to see if we were issued an IP number. See comments in Server program */
void ICACHE_FLASH_ATTR network_check_ip(void) {
    struct ip_info ipconfig;

    os_timer_disarm(&network_timer);                // Disarm timer
    wifi_get_ip_info(STATION_IF, &ipconfig);        // Get Wifi info

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
        init_tcp_conn();            // Init tcp connection
    }
    else {
        os_printf("Waiting for IP...\n\r");
        os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
        os_timer_arm(&network_timer, 1000, 0);
    }         
}

/* tcp Data received callback. Apparently we're doing nothing with the data. */    
static void ICACHE_FLASH_ATTR tcpNetworkRecvCb(void *arg, char *data, unsigned short len) {
    struct espconn *tcpconn=(struct espconn *)arg;
    os_printf(data);
    os_printf("\n");
}

/* TCP Successfully connected Call back */
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg) {    
    /* create a connection struct with passed connection data including successfully
    created connection data */
    struct espconn *tcpconn=(struct espconn *)arg;
    /* register a call back function for when data is received */
    espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);
    
    os_printf("TCP connected\n\r"); 
    tcp_conn_ok = TRUE;
}

/* TCP Reconnected Callback. Called when the connection was reestablished 
When this is called is unclear.*/
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err) {
    os_printf("TCP reconnect\n\r");
    tcp_conn_ok = FALSE;
    /* Reset the network connections */
    network_init();
}

/* TODO what the F is this and the previous function doing */
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg) {
    os_printf("TCP disconnect\n\r");
    tcp_conn_ok = FALSE;
    network_init();
}

/* Initialize the TCP connection */
static void ICACHE_FLASH_ATTR init_tcp_conn(void) {
    global_tcp_connect.type=ESPCONN_TCP;                     // We want to make a TCP connection
    global_tcp_connect.state=ESPCONN_NONE;                   // Set default state to none
    global_tcp_connect.proto.tcp=&global_tcp;                // Give a pointer to our TCP var
    global_tcp_connect.proto.tcp->local_port=espconn_port(); // Ask a free local port to the API
    global_tcp_connect.proto.tcp->remote_port=80;          // Set remote port (bcbcostam)
    // IP to send data to 184.106.153.149   
    global_tcp_connect.proto.tcp->remote_ip[0]=184;      // Your computer IP
    global_tcp_connect.proto.tcp->remote_ip[1]=106;      // Your computer IP
    global_tcp_connect.proto.tcp->remote_ip[2]=153;      // Your computer IP
    global_tcp_connect.proto.tcp->remote_ip[3]=149;      // Your computer IP

    espconn_regist_connectcb(&global_tcp_connect, tcpNetworkConnectedCb);   // Register connect callback
    espconn_regist_disconcb(&global_tcp_connect, tcpNetworkDisconCb);       // Register disconnect callback
    espconn_regist_reconcb(&global_tcp_connect, tcpNetworkReconCb);         // Register reconnection function
    espconn_connect(&global_tcp_connect);                                   // Start connection
}
