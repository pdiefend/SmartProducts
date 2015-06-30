/* Modified from cnlohr */
/* https://github.com/cnlohr/ws2812esp8266/blob/master/user/ws2812.c */

#ifndef _WS2812_H
#define _WS2812_H
#define WSGPIO 2
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "gpio.h"

//You will have to os_intr_lock(); os_intr_unlock();
void WS2812OutBuffer( uint8_t * buffer, uint16_t length );

void setWS2812color(uint8_t red, uint8_t green, uint8_t blue);

#endif