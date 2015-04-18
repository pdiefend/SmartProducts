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
static volatile os_timer_t uploadTimer;
static volatile os_timer_t networkTimer;

// User variables
static uint32_t counter = 0;
static uint32_t total = 0;
static uint16_t lastZero = 0;

// Forward Declarations
static void user_procTask(os_event_t *events);
void uploadTimerISR(void *arg);
void buttonISR(int8_t key);
void user_init();
void networkInit();
void network_check_ip(void);
static void tcpNetworkRecvCb(void *arg, char *data, unsigned short len);
static void tcpNetworkConnectedCb(void *arg);
static void tcpNetworkReconCb(void *arg, sint8 err);
static void tcpNetworkDisconCb(void *arg);
static void init_tcp_conn(void);

/* ISR to handle the 7.5 sec timer which sends the number of button presses to thingspeak.com */
void uploadTimerISR(void *arg) {

    // I really need a testbed...

    /* Things to detect and how to detect them:
    ** 1) Normal Flush
    **      Just keep doing what we're doing, wait for water to start to flow,
    **      and count the pulses until the water stops and report the count
    **
    ** 2) Catastrophic failure (continuous running)
    **      Same as above, only add a counter for number of times the ISR has been
    **      called since the flush event has ended. Report this as a catastrophoic
    **      Failure. I guess the best option is to report the flow every minute until
    **      the failure is cleared.
    **
    ** 3) Slow Leak (Pulsing refill)
    **      Add a threshold to the normal flush. If we are below the threshold: (val << thres),
    **      then we are probably in a slow leak mode. I think we should add an array of the last
    **      <insert arbitrary number here, 10?> and if they are all under the threshold then
    **      we are definately in the slow leak mode. 
    */

    uint32_t temp = total;

    // Win the race (condition) to store the data to be sent    
    ETS_GPIO_INTR_DISABLE();
    total += counter;
    counter = 0;
    ETS_GPIO_INTR_ENABLE();

    // Debug info    
    os_printf("Temp = %d; Total = %d\r\n", temp, total);

    uint32_t tmp2 = total - temp;        
    os_printf("Button was pressed %d Times\r\n", tmp2);

    // prep the data to be sent to Thingspeak    
    uint8_t data[100] = "GET /update?api_key=4U8GOF201NG3X7WM&field1=00000\r\n\r\n";

    if (total != 0 && total == temp){
        // send data
        os_printf("Sending Data\r\n");

        total *= 225;
        total /= 100;

        // swap these lines between button and flow meter.        
        // uint32_t drop_copy = total / 100;
        uint32_t drop_copy = total;

        // input sanitizing
        if (drop_copy < 0) drop_copy = 0;
        if(drop_copy > 99999) drop_copy = 99999;

        // Load the data
        data[44] = (drop_copy / 10000) + 48; // get the thousands place then offset by zer0 in   unicode
        drop_copy = drop_copy % 10000;       // drop the thousands place
        data[45] = (drop_copy / 1000) + 48; // get the thousands place then offset by zer0 in   unicode
        drop_copy = drop_copy % 1000;       // drop the thousands place
        data[46] = (drop_copy / 100) + 48;  // get the hundreds place then offset by zero in    unicode
        drop_copy = drop_copy % 100;        // drop the hundreds place
        data[47] = (drop_copy / 10) + 48;   // get the tens place then offset by zero in unicode
        drop_copy = drop_copy % 10;         // drop the tens place
        data[48] =  drop_copy + 48;         // only the ones remain, offset and put them in.
    
        // Send the data
        espconn_sent(&global_tcp_connect, data, 100);
        os_printf(data);

        // reset the total        
        total = 0;
    } else if (total == 0) {
        // total is 0 and there is nothing worth sending
        os_printf("Nothing to be sent\r\n");
    } else if (total != temp){
        // or total has changed and there may be more data to send
        os_printf("Awaiting new data\r\n");
    }
 
    // previous attempts for prosperity's sake...
    //uint8_t data[100] = "GET /update?api_key=4U8GOF201NG3X7WM&field1=10 HTTP/1.0\r\nHost: http://api.thingspeak.com\r\n\r\n";
    //uint8_t data[20] = "https://api.thingspeak.com/update?api_key=4U8GOF201NG3X7WM%20&field1=8"
}

/* Do nothing function */
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(1000);
}

/* ISR to handle positive edges of the interrupt pin */
void buttonISR(int8_t key) {
    counter++;
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));
}


/* User Init, code execution starts here from OS */
void ICACHE_FLASH_ATTR user_init() {
    uart_init(BIT_RATE_115200, BIT_RATE_115200);                    // Init UART @ 115200 bps    
    
    ETS_GPIO_INTR_DISABLE();                                        // Disable Interrupts
    gpio_init();                                                    // Enable GPIOS
    ETS_GPIO_INTR_ATTACH(buttonISR, 0);                             // GPIO0 interrupt handler
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO0);             // Set GPIO0 function
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(0));                       // Set GPIO0 as input
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));               // Clear GPIO0 status
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_POSEDGE); // Interrupt on posedge
    ETS_GPIO_INTR_ENABLE();                                         // Enable interrupts

    //Setup timer
    os_timer_disarm(&uploadTimer);
    os_timer_setfn(&uploadTimer, (os_timer_func_t *)uploadTimerISR, NULL);
    os_timer_arm(&uploadTimer, 7500, 1); // every 30 seconds, max is 15 seconds this will change
    // for the IoToliet app to have a different program flow entirely with data sent as it is useful
    
    networkInit();
    char ssid[32] = SSID;
    char password[64] = PASS;
    struct station_config stationConf;
    wifi_set_opmode(0x1);               // Client-only mode
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);
    wifi_station_set_config(&stationConf);
    wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);     // Set GPIO2 as WiFi status LED

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");
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
    os_printf(data);
    os_printf("\n");
}

/* tcp Successfully connected Call back */
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg) {    
    struct espconn *tcpconn=(struct espconn *)arg;
    
    // register the call back function for when data is received
    espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);    

    os_printf("TCP connected\n\r"); 
    tcp_conn_ok = TRUE;
}

/* Callback Function for when the network was reconnected, re-init stuff */
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err) {
    os_printf("TCP reconnect\n\r");
    tcp_conn_ok = FALSE;
    /* Reset the network connections */
    networkInit();
}

/* Callback Function for when the network was disconnected, re-init stuff */
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg) {
    os_printf("TCP disconnect\n\r");
    tcp_conn_ok = FALSE;
    networkInit();
}

/* Initialize the TCP connection */
static void ICACHE_FLASH_ATTR init_tcp_conn(void) {
    global_tcp_connect.type=ESPCONN_TCP;
    global_tcp_connect.state=ESPCONN_NONE;
    global_tcp_connect.proto.tcp=&global_tcp;
    global_tcp_connect.proto.tcp->local_port=espconn_port();

    // IP to send data to 184.106.153.149   
    global_tcp_connect.proto.tcp->remote_ip[0]=184;
    global_tcp_connect.proto.tcp->remote_ip[1]=106;
    global_tcp_connect.proto.tcp->remote_ip[2]=153;
    global_tcp_connect.proto.tcp->remote_ip[3]=149;
    global_tcp_connect.proto.tcp->remote_port=80;

    // Register Callbacks
    espconn_regist_connectcb(&global_tcp_connect, tcpNetworkConnectedCb);
    espconn_regist_disconcb(&global_tcp_connect, tcpNetworkDisconCb);
    espconn_regist_reconcb(&global_tcp_connect, tcpNetworkReconCb);

    // Fire it up
    espconn_connect(&global_tcp_connect);
}
