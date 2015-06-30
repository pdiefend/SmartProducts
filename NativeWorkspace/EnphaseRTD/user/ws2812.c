/* Modified from cnlohr */
/* https://github.com/cnlohr/ws2812esp8266/blob/master/user/ws2812.c */

#include "ws2812.h"
#include "ets_sys.h"
//#include "mystuff.h"
#include "osapi.h"
#include "gpio.h"

#define GPIO_OUTPUT_SET(gpio_no, bit_value) \
	gpio_output_set(bit_value<<gpio_no, ((~bit_value)&0x01)<<gpio_no, 1<<gpio_no,0)


//I just used a scope to figure out the right time periods.

void SEND_WS_0()
{
	uint8_t time = 8;
	//WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
	//WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
    gpio_output_set(0, BIT2, BIT2, 1);
    gpio_output_set(0, BIT2, BIT2, 1);
	while(time--)
	{
		//WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
        gpio_output_set(0, BIT2, BIT2, 0);
	}

}

void SEND_WS_1()
{
	uint8_t time = 9;
	while(time--)
	{
		//WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 1 );
        gpio_output_set(0, BIT2, BIT2, 1);
	}
	time = 3;
	while(time--)
	{
		//WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(WSGPIO), 0 );
        gpio_output_set(0, BIT2, BIT2, 0);
	}

}


void WS2812OutBuffer( uint8_t * buffer, uint16_t length )
{
	uint16_t i;
	GPIO_OUTPUT_SET(GPIO_ID_PIN(WSGPIO), 0);
	for( i = 0; i < length; i++ )
	{
		uint8_t byte = buffer[i];
		if( byte & 0x80 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x40 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x20 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x10 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x08 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x04 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x02 ) SEND_WS_1(); else SEND_WS_0();
		if( byte & 0x01 ) SEND_WS_1(); else SEND_WS_0();
	}
	//reset will happen when it's low long enough.
	//(don't call this function twice within 10us)
    os_delay_us(11);
}

void setWS2812color(uint8_t red, uint8_t green, uint8_t blue){
    
    ets_wdt_disable();
    ETS_GPIO_INTR_DISABLE();
    os_printf("\r\n====================== 1\r\n");
    GPIO_OUTPUT_SET(GPIO_ID_PIN(WSGPIO), 0);
    os_printf("\r\n====================== 2\r\n");
    if( green & 0x80 ) SEND_WS_1(); else SEND_WS_0();
    if( green & 0x40 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x20 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x10 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x08 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x04 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x02 ) SEND_WS_1(); else SEND_WS_0();
	if( green & 0x01 ) SEND_WS_1(); else SEND_WS_0();
    
    os_delay_us(11);
    os_printf("\r\n====================== 3\r\n");
    if( red & 0x80 ) SEND_WS_1(); else SEND_WS_0();
    if( red & 0x40 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x20 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x10 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x08 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x04 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x02 ) SEND_WS_1(); else SEND_WS_0();
	if( red & 0x01 ) SEND_WS_1(); else SEND_WS_0();
    
    os_delay_us(11);
    os_printf("\r\n====================== 4\r\n");
    if( blue & 0x80 ) SEND_WS_1(); else SEND_WS_0();
    if( blue & 0x40 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x20 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x10 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x08 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x04 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x02 ) SEND_WS_1(); else SEND_WS_0();
	if( blue & 0x01 ) SEND_WS_1(); else SEND_WS_0();
    
    os_printf("\r\n====================== 5\r\n");
    ETS_GPIO_INTR_ENABLE();
    os_delay_us(11); // In case it is called immediately again.
    /*
    uint8_t buffer[3];
    buffer[0] = green;
    buffer[1] = red;
    buffer[2] = blue;

    
    ets_wdt_disable();
    ETS_GPIO_INTR_DISABLE();
    
    ETS_GPIO_INTR_ENABLE();
    ets_wdt_enable();
    */
}

