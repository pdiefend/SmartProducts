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

/*
Network init forward declaration. This kind of stuff should really be in the header file...
*/
void ICACHE_FLASH_ATTR network_init();

/*
The ESP8266 SDK creates an RTOS were we have to create a task Queue for events to exectue
brush up on your RTOS skills if you have any.
*/
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

/*
This is a forward declaration for the idle (or do-nothing) task
*/
static void user_procTask(os_event_t *events);

/*
Timer for stuff... TODO
*/
LOCAL os_timer_t network_timer;


static esp_udp global_udp;                                  // UDP connect var (see espconn.h)
static struct espconn global_udp_connect;                   // Connection struct (see espconn.h)
static uint8_t udp_conn_ok = FALSE;                         // Bool to know if udp connection set

/*
Call back function to call when a UDP packet is received.
*/
static void ICACHE_FLASH_ATTR udpNetworkRecvCb(void *arg, char *data, unsigned short len) 
{
    /* Create a esp connection struct from the pointer passed to us */
    struct espconn *udpconn=(struct espconn *)arg;
    
    /* Okay now we are getting to some of my edits.
    If the data is a character that represents a number */
    if(data[0] & 0x30)
    {
    
    /* Obviously print the data to the UART */
    os_printf("DATA!!!! ");
    os_printf("%d", data[0]);
    os_printf("\n\r");    

    data[0] &= 0x7F;

    /* Numbers are wierd since I wasn't paying attention to above bit mask */
        if (data[0] < 50)
        {
        /* LOW */
	gpio_output_set(0, BIT2, BIT2, 0);
        }
        else
        {
        /* HIGH */
            gpio_output_set(BIT2, 0, BIT2, 0);
        }
    }
    else
    {
        gpio_output_set(BIT2, 0, BIT2, 0);
    }


}


/*
Fire up the networking portion of the ESP
*/
void ICACHE_FLASH_ATTR network_start(void) 
{   
    global_udp_connect.type=ESPCONN_UDP;              // We want to make a UDP connection
    global_udp_connect.state=ESPCONN_NONE;                      // Set default state to none
    global_udp_connect.proto.udp=&global_udp;                   // Give a pointer to our UDP var
    global_udp_connect.proto.udp->local_port=2222;              // Set local port to 2222
    global_udp_connect.proto.udp->remote_port=2222;             // Set remote port
    
    /*??????*/
    global_udp_connect.proto.udp->remote_ip[0]=2; // The other ESP8266 IP
    
    if(espconn_create(&global_udp_connect) == 0)                // Correctly setup
    {
        espconn_regist_recvcb(&global_udp_connect, udpNetworkRecvCb);
        os_printf("UDP connection set\n\r");
        udp_conn_ok = TRUE;
    }
}

/*
Make sure we are connected to the network and then start up the network functionality.
*/
void ICACHE_FLASH_ATTR network_check_ip(void)
{
    struct ip_info ipconfig;

    os_timer_disarm(&network_timer);                // Disarm timer that calls this function
    wifi_get_ip_info(STATION_IF, &ipconfig);        // Get Wifi info

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) 
    {
        network_start();                            // Everything in order
    }
    else 
    {
        os_printf("Waiting for IP...\n\r");
	/* rearm the timer if failed. See comment in network init, but why do it this way? */
        /* Is there a bug in that function that he's trying to work around? */
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
void ICACHE_FLASH_ATTR network_init()
{       
    /* Disarm the (NULL?) timer, Why disarm? TODO */
    os_timer_disarm(&network_timer);
    /* Set the function to be triggered by the timer */
    os_timer_setfn(&network_timer, (os_timer_func_t *)network_check_ip, NULL);
    /* Arm the timer, non repeating... Why not just disarm after success? Is that not possible TODO */
    os_timer_arm(&network_timer, 1000, 0);
}

//Init function 
void ICACHE_FLASH_ATTR user_init()
{    
    uart_init(BIT_RATE_57600, BIT_RATE_57600);                    // Init UART @ 57600 bps    
    
    gpio_init();                                              // Initialize the GPIO subsystem.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);                // Set GPIO2 function
    gpio_output_set(0, BIT2, BIT2, 0);                                  // Set GPIO2 low output
    
    network_init();                                                     // Init network timer
    
    char ssid[32] = SSID;                                 // Wifi SSID
    char password[64] = PASS;                                // Wifi Password
    struct station_config stationConf;                              // Station conf struct    
    wifi_set_opmode(0x1);                                               // Set station mode
    os_memcpy(&stationConf.ssid, ssid, 32);                             // Set settings
    os_memcpy(&stationConf.password, password, 64);                     // Set settings
    wifi_station_set_config(&stationConf);                              // Set wifi conf
    
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");                                  // Startup done
}
