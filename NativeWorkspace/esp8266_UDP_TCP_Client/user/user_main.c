/* From a guy named Limpkin who has a blog at
 http://www.limpkin.fr/index.php?post/2014/12/07/First-Steps-with-the-ESP8266-03-Development-Board
*/

// Limpkin's comments are line comments

/* 
My comments are block comments
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

/* forward declaration of network init function */
void ICACHE_FLASH_ATTR network_init();

/* RTOS Process Queue */
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
/* forward declaration of the do nothing task */
static void user_procTask(os_event_t *events);

/* I think he may have started from blinky... */
/* Timer declarations */
static volatile os_timer_t led_timer;
LOCAL os_timer_t network_timer;

static esp_udp global_udp;                              // UDP connect var (see espconn.h)
static struct espconn global_udp_connect;               // Connection struct (see espconn.h)
static esp_tcp global_tcp;                              // TCP connect var (see espconn.h)
static struct espconn global_tcp_connect;               // Connection struct (see espconn.h)
static uint8_t udp_conn_ok = FALSE;                     // Bool to know if udp connection set
static uint8_t tcp_conn_ok = FALSE;                     // Bool to know if tcp connection set
static uint16_t doppler_counter = 0;                    // Doppler counter

/* itoa is a classic integer to string function that I guess the ESP does not have */
/* Well, this is going to be... special */
char* itoa(int i, char b[])
{
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

/* A good example of a timed function that does all sorts of application specific things */
void led_timerfunc(void *arg)
{
    /* Not that this has anything to do with using the ESP with its network functionality
    But it looks like he trying to handle a race condition. If you don't know what that is
    you should look it up along with an RTOS */
    ETS_GPIO_INTR_DISABLE();                        // Disable gpio interrupts
    uint16_t drop_copy = doppler_counter;           // copy doppler val
    doppler_counter = 0;                            // not good!
    ETS_GPIO_INTR_ENABLE();                         // Enable gpio interrupts
    
    /* Poll the GPIOs */
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2)     // If GPIO2 high
    {
        gpio_output_set(0, BIT2, BIT2, 0);          // Set GPIO2 to LOW
    }
    else                                            // Else
    {
        gpio_output_set(BIT2, 0, BIT2, 0);          // Set GPIO2 to HIGH
    }

    /* If a button is pressed tell something at the end of the TCP connection it was pressed.
    I believe he had a computer in this example as well. */
   if ((GPIO_INPUT_GET(13) == 0) && (tcp_conn_ok == TRUE))
    {
        uint8_t data[20] = "\r\nButton pressed!\r\n";
        espconn_sent(&global_tcp_connect, data, 20);
    }
    /* Send the data from his doppler sensor */
    /* I guess this guy just happened to have a doppler sensor lying around. Aren't those $$$$? */ 
    if (tcp_conn_ok == TRUE)
    {
        uint8_t data[10] = " \x00\x00\x00\x00\x00";
        itoa(drop_copy, data+1);
        espconn_sent(&global_tcp_connect, data, 10);
    }
    /* If UDP is good to go (the other ESP) send the data along. */
    if (udp_conn_ok == TRUE)
    {
        /* Prep the data with some user set framing (bit7 is a 1) Why */
        uint8_t data = (GPIO_INPUT_GET(13) << 7) + (drop_copy / 2);
        /* this is a way easier method than the UART + Arduino */
        espconn_sent(&global_udp_connect, &data, 1);
    }
}

/* A good exmaple of an interrupt handler */
void doppler_int_handler(int8_t key)
{
    // Increment counter
    doppler_counter++;
    
    //Not that sure what this does yet and where the register is used for
    /* Bad programmer. read pg 56 of the sdk doc */        
    /* Read the interrupt status register. pg 56 of the sdk doc */
    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

    //clear interrupt status
    /* Clear the interrupt for only your vector hence the read above and bit mask */    
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(12));
}

/* tcp Data received callback. Apparently we're doing nothing with the data. */    
static void ICACHE_FLASH_ATTR tcpNetworkRecvCb(void *arg, char *data, unsigned short len) 
{
    struct espconn *tcpconn=(struct espconn *)arg;
}

/* TCP Successfully connected Call back */
static void ICACHE_FLASH_ATTR tcpNetworkConnectedCb(void *arg) 
{    
    /* create a connection struct with passed connection data including successfully
    created connection data */
    struct espconn *tcpconn=(struct espconn *)arg;
    /* register a call back function for when data is received */
    espconn_regist_recvcb(tcpconn, tcpNetworkRecvCb);
    
    os_printf("TCP connected\n\r"); 
    uint8_t data[20] = "Hello you!\r\n";
    /* Again, wayyyy easier than the UART methodology */
    espconn_sent(tcpconn, data, 20);
    tcp_conn_ok = TRUE;
}

/* TCP Reconnected Callback. Called when the connection was reestablished 
When this is called is unclear.*/
static void ICACHE_FLASH_ATTR tcpNetworkReconCb(void *arg, sint8 err) 
{
    os_printf("TCP reconnect\n\r");
    tcp_conn_ok = FALSE;
    /* Reset the network connections */
    network_init();
}

/* TODO what the F is this and the previous function doing */
static void ICACHE_FLASH_ATTR tcpNetworkDisconCb(void *arg) 
{
    os_printf("TCP disconnect\n\r");
    tcp_conn_ok = FALSE;
    network_init();
}

/* Initialize the TCP connection */
static void ICACHE_FLASH_ATTR init_tcp_conn(void) 
{
    global_tcp_connect.type=ESPCONN_TCP;                     // We want to make a TCP connection
    global_tcp_connect.state=ESPCONN_NONE;                   // Set default state to none
    global_tcp_connect.proto.tcp=&global_tcp;                // Give a pointer to our TCP var
    global_tcp_connect.proto.tcp->local_port=espconn_port(); // Ask a free local port to the API
    global_tcp_connect.proto.tcp->remote_port=7778;          // Set remote port (bcbcostam)
    /* IP to send data to... I think */    
    //global_tcp_connect.proto.tcp->remote_ip[0]=xx;      // Your computer IP
    //global_tcp_connect.proto.tcp->remote_ip[1]=xx;      // Your computer IP
    //global_tcp_connect.proto.tcp->remote_ip[2]=xx;      // Your computer IP
    //global_tcp_connect.proto.tcp->remote_ip[3]=xx;      // Your computer IP

    espconn_regist_connectcb(&global_tcp_connect, tcpNetworkConnectedCb);   // Register connect callback
    espconn_regist_disconcb(&global_tcp_connect, tcpNetworkDisconCb);       // Register disconnect callback
    espconn_regist_reconcb(&global_tcp_connect, tcpNetworkReconCb);         // Register reconnection function
    espconn_connect(&global_tcp_connect);                                   // Start connection
}

/* Initialize the UDP connection */
static void ICACHE_FLASH_ATTR init_udp_conn(void) 
{   
    global_udp_connect.type=ESPCONN_UDP;                  // We want to make a UDP connection
    global_udp_connect.state=ESPCONN_NONE;                // Set default state to none
    global_udp_connect.proto.udp=&global_udp;             // Give a pointer to our UDP var
    global_udp_connect.proto.udp->local_port=2222;        // Set local port
    global_udp_connect.proto.udp->remote_port=2222;       // Set remote port
    global_udp_connect.proto.udp->remote_ip[0]=192;        // The other ESP8266 IP
    global_udp_connect.proto.udp->remote_ip[1]=168;        // The other ESP8266 IP
    global_udp_connect.proto.udp->remote_ip[2]=254;        // The other ESP8266 IP
    global_udp_connect.proto.udp->remote_ip[3]=2;        // The other ESP8266 IP
    if(espconn_create(&global_udp_connect) == 0)          // Correctly setup
    {
        os_printf("UDP connection set\n\r");
        udp_conn_ok = TRUE;
    }
}

/* Start network connections */
void ICACHE_FLASH_ATTR network_start(void) 
{   
    init_tcp_conn();            // Init tcp connection
    if(udp_conn_ok == FALSE)    // If UDP connection hasn't been setup
    {
        init_udp_conn();
    }
}

/* Check to see if we were issued an IP number. See comments in Server program */
void ICACHE_FLASH_ATTR network_check_ip(void)
{
    struct ip_info ipconfig;

    os_timer_disarm(&network_timer);                // Disarm timer
    wifi_get_ip_info(STATION_IF, &ipconfig);        // Get Wifi info

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) 
    {
        network_start();                            // Everything in order
    }
    else 
    {
        os_printf("Waiting for IP...\n\r");
        os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
        os_timer_arm(&network_timer, 1000, 0);
    }         
}

//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    os_delay_us(10);
}

// network init function
/* See comments in other program */
void ICACHE_FLASH_ATTR network_init()
{       
    os_timer_disarm(&network_timer);
    os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
    os_timer_arm(&network_timer, 1000, 0);
}

//Init function 
void ICACHE_FLASH_ATTR user_init()
{    
    uart_init(BIT_RATE_57600, BIT_RATE_57600);    // Init UART @ 57600 bps    
                                                  
    ETS_GPIO_INTR_DISABLE();                      // Disable gpio interrupts
    gpio_init();                                  // Initialize the GPIO subsystem.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);    // Set GPIO2 function
    gpio_output_set(0, BIT2, BIT2, 0);                      // Set GPIO2 low output
    ETS_GPIO_INTR_ATTACH(doppler_int_handler, 12);          // GPIO12 interrupt handler
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);    // Set GPIO12 function
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(12));              // Set GPIO12 as input
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(12));      // Clear GPIO12 status
    gpio_pin_intr_state_set(GPIO_ID_PIN(12), 3);            // Interrupt on any GPIO12 edge
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);    // Set GPIO13 function
    gpio_output_set(0, 0, 0, BIT13);                        // Set GPIO13 as input
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_MTCK_U);                 // Disable pulldown
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);                   // Enable pullup
    ETS_GPIO_INTR_ENABLE();                                 // Enable gpio interrupts
    
    os_timer_disarm(&led_timer);                            // Disarm led timer
    os_timer_setfn(&led_timer, (os_timer_func_t *)led_timerfunc, NULL); // Setup led timer
    os_timer_arm(&led_timer, 500, 1);                       // Arm led timer, 0.5sec, repeat
    
    network_init();                                         // Init network timer
    
    char ssid[32] = "xxxxxxxxxxxx!";                        // Wifi SSID
    char password[64] = "xxxxxxxxxxx";                      // Wifi Password
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
