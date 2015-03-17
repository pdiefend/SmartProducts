#include "c_types.h"
//#include "ip_addr.h"
#include "ets_sys.h"
//#include "espconn.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static volatile os_timer_t some_timer;

static uint16_t counter = 0;

void some_timerfunc(void *arg)
{
    //Do blinky stuff
    if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2)
    {
        //Set GPIO2 to LOW
        gpio_output_set(0, BIT2, BIT2, 0);
    }
    else
    {
        //Set GPIO2 to HIGH
        gpio_output_set(BIT2, 0, BIT2, 0);
    }
    ETS_GPIO_INTR_DISABLE();                        // Disable gpio interrupts
    uint16_t drop_copy = counter;
    ETS_GPIO_INTR_ENABLE();                         // Enable gpio interrupts

    os_printf("Button Pressed %d Times", drop_copy);
}

//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events) {
    os_delay_us(10);
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


    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);

    ETS_GPIO_INTR_ATTACH(buttonISR, 0);          // GPIO12 interrupt handler
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO0);    // Set GPIO12 function
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(0));              // Set GPIO12 as input
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));      // Clear GPIO12 status
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), 1);            // Interrupt on posedge

    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);                   // Enable pullup
    ETS_GPIO_INTR_ENABLE();                                 // Enable gpio interrupts

    //Disarm timer
    os_timer_disarm(&some_timer);

    //Setup timer
    os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);

    //Arm the timer
    //&some_timer is the pointer
    //1000 is the fire time in ms
    //0 for once and 1 for repeating
    os_timer_arm(&some_timer, 1000, 1);
    
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    os_printf("\n\rStartup done\n\r");                                  // Startup done
}
