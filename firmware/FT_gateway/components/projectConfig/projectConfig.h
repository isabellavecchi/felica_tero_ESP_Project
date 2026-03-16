/**
 * @file projectConfig.h
 * @author your name (you@domain.com)
 * @brief 
 * @details
 * @version 0.1
 * @date 2025-05-21
 * 
 * @copyright Public Domain (or CC0 licensed, at your option.) 2025
 * 
 */

#ifndef __PROJECT_CONFIG_LIB__
#define __PROJECT_CONFIG_LIB__

#define TRUE    1
#define FALSE   0

#define BOARD_ESP32C6
// #define BOARD_ESP32S3

#if defined BOARD_ESP32C6
    #define CREATE_TASK xTaskCreate
#elif defined BOARD_ESP32S3
    #define CREATE_TASK xTaskCreateStaticPinnedToCore
#endif

// RGB LED GPIOs
#define RGB_LED_RED_GPIO	12
#define RGB_LED_GREEN_GPIO	13
#define RGB_LED_BLUE_GPIO	14
    // RGB LED color mix channels
#define RGB_LED_CHANNEL_NUM	3 ///> qty of led rgb pins

// WI-FI
#define WIFI_AP_SSID			"WAKE_N_DANCE"		// Access Point SSID
#define WIFI_AP_PASSWORD		"12345678"			// AP password
#define WIFI_AP_CHANNEL			1					// AP channel
#define WIFI_AP_SSID_HIDDEN		0					// AP visible = 0
#define WIFI_AP_MAX_CONNECTIONS	5					// how many people can connect to AP
#define WIFI_AP_BEACON_INTERVAL	100					// MAYBE IT SHOULD BE BIGGER FOR NOT GETTING IN THE WAY OF SONG
#define WIFI_AP_IP				"192.168.0.1"		// AP default IP
#define WIFI_AP_GATEWAY			"192.168.0.1"		// AP default gateway
#define WIFI_AP_NETMASK			"255.255.255.0"     // AP netmask
#define WIFI_AP_BANDWIDTH		WIFI_BW_HT20		// AP bandwidth = 20 MHz, the 40 will be for station
#define WIFI_STA_POWER_SAVE		WIFI_PS_NONE		// Power save is not being used
#define WIFI_SSID_LENGTH		32					// IEEE standard maximum
#define WIFI_PASSWORD_LENGTH	64					// IEEE standard maximum
#define MAX_CONNECTION_RETRIES	5					// retry numbers on disconnect

#endif //__PROJECT_CONFIG_LIB__