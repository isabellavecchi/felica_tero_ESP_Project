/**
 * @file wifiApp.c
 * @brief 
 * @details
 * @date 16 de nov. de 2024
 * @author Isabella Vecchi
 */


/**************************
**		  INCLUDES	 	 **
**************************/

// C libraries
#include <stdint.h>

// ESP libraries
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_interface.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

// Personal libraries
#include "wifiApp.h"
#include "projectConfig.h"
#include "tasks_common.h"
#include "hal_memory.h"
#include "hal_system.h"


/**************************
**		DECLARATIONS	 **
**************************/

	/* Variables */

// Tag used for ESP serial console messages
static const char TAG[] = "wifi_app";

// Wifi connected events callbacks
static wifi_fn_callbacks_t wifi_fn_callbacks = {0};

// Alocating Station WiFi credencials
static wifiApp_credentials_t wifi_credentials = {0};
static mem_address_saver_t wifi_credentials_memorizer =
{
	.partition_name	= NVS_DEFAULT_PART_NAME,
	.namespace		= WIFI_APP_NAMESPACE,
	.storage_key	= WIFI_APP_KEY,
	.obj_addr		= &wifi_credentials,
	.obj_syze		= sizeof(wifiApp_credentials_t),
	.toSave			= FALSE
};

// Used for returning the WiFi configuration
static wifi_config_t wifi_config_v;

// Used to track the number for retries when a connectiona attempt fails
static uint8_t g_retry_number;
static wifi_connect_status_t connectStatus = 0;

/**
 * @brief WiFi application event group handle and status bits
 */
static EventGroupHandle_t wifi_app_event_group;
const int WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT			= BIT0;
const int WIFI_APP_TRY_TO_CONNECT_BIT						= BIT1;
const int WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT		= BIT2;

static EventBits_t eventBits;

// Netif objects for the station and access point
static esp_netif_t * esp_netif_sta = NULL;
static esp_netif_t * esp_netif_ap  = NULL;

// Variable to check if the state Id is valid
static uint8_t g_qtd_states;


	/* FreeRTOS Structure */

// Queue handle used to manipulate the main queue of events
static QueueHandle_t wifi_app_queue_handle_t;


	/* Static Functions */
	
// STATE MACHINE
static void WIFI_STATE_FUNC_NAME(WIFI_APP_SIGNAL_READY)(wifi_app_queue_message_t * st);
static void WIFI_STATE_FUNC_NAME(WIFI_APP_TRY_TO_CONNECT)(wifi_app_queue_message_t * st);
static void WIFI_STATE_FUNC_NAME(WIFI_APP_STA_CONNECTED_GOT_IP)(wifi_app_queue_message_t * st);
static void wifiApp_stateMachine_handler(wifi_app_queue_message_t * msg);

// STA & AP FUNCTIONS
static void wifiApp_event_handler(void * arg_p, esp_event_base_t event_base, int32_t eventId, void * eventData_p);
static void wifiApp_eventHandler_init(void);
static void wifiApp_defaultWifi_init(void);
static void wifiApp_configMode(wifi_mode_t wifi_mode);
static void wifiApp_onlySTA_config(void);
static void wifiApp_softAP_config(void);
static void wifiApp_sta_connect(void);
static void wifiApp_sta_disconnectedLogInfo(void * eventData_p);

// APP FUNCTIONS
static void wifiApp_setup(void);
static void wifiApp_task(void * pvParameters);
static void wifiApp_setRealCredentialsToConnect(void);

// MEMORY FUNCTIONS
static esp_err_t wifiApp_loadCredentials(void);
static void wifiApp_saveCredentials(void);
static void wifiApp_forgetCredentials(void);


/**************************
**		  GETTERS		 **
**************************/

// Gets the WiFi connection status
uint8_t wifiApp_getConnStatus(void)
{
	return connectStatus;
}


esp_err_t wifiApp_getWifiConnectInfo(char * out_ssid, char * out_ip, char * out_netmask, char * out_gateway)
{
	esp_err_t result = ESP_FAIL;

	wifi_ap_record_t wifi_data;
	if (ESP_OK == esp_wifi_sta_get_ap_info(&wifi_data))
	{
		sprintf(out_ssid, "%s", (char*)wifi_data.ssid);
		
		esp_netif_ip_info_t ip_info;
		if (ESP_OK == esp_netif_get_ip_info(esp_netif_sta, &ip_info))
		{
			esp_ip4addr_ntoa(&ip_info.ip,		out_ip, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.netmask,	out_netmask, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.gw,		out_gateway, IP4ADDR_STRLEN_MAX);
			
			result = ESP_OK;
		}
	}
	return result;
}


/**************************
**		  SETTERS		 **
**************************/

// Sets the WiFi credentials info
void wifiApp_setCredentials(char * ssid, char * passwd)
{
	memset(&wifi_credentials, 0x00, sizeof(wifiApp_credentials_t));

    sprintf(wifi_credentials.ssid,		"%s", ssid);
    sprintf(wifi_credentials.passwd,	"%s", passwd);
}


static void wifiApp_setRealCredentialsToConnect(void)
{
	memset(&wifi_config_v, 	  0x00, sizeof(wifi_config_t));

	memcpy(&(wifi_config_v.sta.ssid),		&(wifi_credentials.ssid),	WIFI_SSID_LENGTH);
	memcpy(&(wifi_config_v.sta.password),	&(wifi_credentials.passwd),	WIFI_PASSWORD_LENGTH);
}


/**************************
**		APP FUNCTIONS	 **
**************************/

/**
 * @brief function that sets up the WiFi environment
 * configurating WiFi & Soft Access and initializing TCP/IP
 * @details
 */
static void wifiApp_setup(void)
{
	wifi_mode_t	 wifi_mode = 0;

	// Initialize the event handler
	wifiApp_eventHandler_init();
	
	// Initialize the TCP/IP stack and WiFi config
	wifiApp_defaultWifi_init();

	// If it already has SSID and passwd of network configured, it doesn't need AP.
	// if (ESP_OK == wifiApp_loadCredentials())
	// {
	// 	wifi_mode = WIFI_MODE_STA;
	// }
	// else
	// {
	// 	wifi_mode = WIFI_MODE_APSTA;
	// }

	wifiApp_configMode(WIFI_MODE_APSTA);
}

// Sets the callbacks functions
void wifiApp_setCallbacks(
	wifi_event_callback_t signal_ready_cb,
	wifi_event_callback_t sta_connected_cb
)
{
	if (NULL != signal_ready_cb)
	{
		wifi_fn_callbacks.signal_ready_cb = signal_ready_cb;
	}

	if (NULL != sta_connected_cb)
	{
		wifi_fn_callbacks.sta_connected_cb = sta_connected_cb;
	}
}



/**************************
**		STATE MACHINE	 **
**************************/

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on 
 * [WIFI_APP_SIGNAL_READY] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_SIGNAL_READY)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_SIGNAL_READY]);
	
	// ledRGB_wifi_disconnected();

	if(wifi_fn_callbacks.signal_ready_cb != NULL)
	{
		wifi_fn_callbacks.signal_ready_cb();
	}
}

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on
 * [WIFI_APP_TRY_TO_CONNECT] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_TRY_TO_CONNECT)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_TRY_TO_CONNECT]);

	xEventGroupSetBits(wifi_app_event_group, WIFI_APP_TRY_TO_CONNECT_BIT);
	
	// Attempt a connection
	wifiApp_sta_connect();
	
	// Set current number of retries to zero
	g_retry_number = 0;
	
	connectStatus = WIFI_STATUS_CONNECTING;
}

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on
 * [WIFI_APP_STA_CONNECTED_GOT_IP] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_STA_CONNECTED_GOT_IP)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_STA_CONNECTED_GOT_IP]);
	
 	// ledRGB_wifi_connected();
	// displayOled_printHeaderNBody("CONNECTED!", "");
	connectStatus = WIFI_STATUS_CONNECT_SUCCESS;
	
	eventBits = xEventGroupGetBits(wifi_app_event_group);

	// Save Sta creds only if connecting from the http server (not loaded from NVS)
	if (eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT)
	{
		// Clear bits, in case we want to disconnect and reconnect, than start again
		xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
	}
	else
	{
		wifiApp_saveCredentials();
		halSys_restart();
	}
	if(eventBits & WIFI_APP_TRY_TO_CONNECT_BIT)
	{
		xEventGroupClearBits(wifi_app_event_group, WIFI_APP_TRY_TO_CONNECT_BIT);
	}

	if(wifi_fn_callbacks.sta_connected_cb != NULL)
	{
		wifi_fn_callbacks.sta_connected_cb();
	}
}

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on
 * [WIFI_APP_USER_REQUESTED_STA_DISCONNECT] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_USER_REQUESTED_STA_DISCONNECT)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_USER_REQUESTED_STA_DISCONNECT]);
	
	xEventGroupSetBits(wifi_app_event_group, WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT);

 	// ledRGB_wifi_disconnect();
	// so it doesn't try to reconnect when we hit the button disconnect
	g_retry_number = MAX_CONNECTION_RETRIES;
	
 	ESP_ERROR_CHECK(esp_wifi_disconnect());
	wifiApp_forgetCredentials();
 	// ledRGB_wifi_disconnected();
}

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on
 * [WIFI_APP_STA_DISCONNECTED] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_STA_DISCONNECTED)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_STA_DISCONNECTED]);

	eventBits = xEventGroupGetBits(wifi_app_event_group);

	if(eventBits & WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT)
	{
		ESP_LOGI(TAG, "WIFI_APP_STA_DISCONNECTED: ATTEMPT USING SAVED CREDENTIALS");
		xEventGroupClearBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
		wifiApp_forgetCredentials();
	}
	else if (eventBits & WIFI_APP_TRY_TO_CONNECT_BIT)
	{
		ESP_LOGI(TAG, "WIFI_APP_STA_DISCONNECTED: ATTEMPT FROM THE HTTP SERVER");
		xEventGroupClearBits(wifi_app_event_group, WIFI_APP_TRY_TO_CONNECT_BIT);
		connectStatus = WIFI_STATUS_CONNECT_SUCCESS;
	}
	else if (eventBits & WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT)
	{
		ESP_LOGI(TAG, "WIFI_APP_STA_DISCONNECTED: USER REQUESTED DISCONECTION");
		xEventGroupClearBits(wifi_app_event_group, WIFI_APP_USER_REQUESTED_STA_DISCONNECT_BIT);
		connectStatus = WIFI_STATUS_DISCONNECTED;
	}
	else
	{
		ESP_LOGI(TAG, "WIFI_APP_STA_DISCONNECTED: ATTEMPT FAILED, CHECK WIFI ACESS POINT AVAILABITITY");
		/// @note Adjust this case to your needs - maybe you want to keep trying to connect...
	}
	
 	// ledRGB_wifi_disconnected();
}

/**
 * @brief State Machine Function Definition according to sm_wifi_app_function
 * function that defines the behavior on
 * [WIFI_APP_LOAD_SAVED_CREDENTIALS] state
 * @details
 */
static void WIFI_STATE_FUNC_NAME(WIFI_APP_LOAD_SAVED_CREDENTIALS)(wifi_app_queue_message_t * st)
{
	ESP_LOGI(TAG, "%s", sm_wifi_app_state_names[WIFI_APP_LOAD_SAVED_CREDENTIALS]);
	if (ESP_OK == wifiApp_loadCredentials())
	{
		ESP_LOGI(TAG, "Loaded station configuration");
		wifiApp_sta_connect();
		xEventGroupSetBits(wifi_app_event_group, WIFI_APP_CONNECTING_USING_SAVED_CREDS_BIT);
	}
	else
	{
		ESP_LOGI(TAG, "Unable to load sation configuration");
	}
	// Next, start the web server
	wifiApp_sendMessage(WIFI_APP_SIGNAL_READY);
}


// State Machine Table Functions - to be used in the loop
sm_wifi_table_fn_t sm_wifi_state_table[] =
{
#define X(ID, ENUM) { WIFI_STATE_FUNC_NAME(ENUM) }, 
	X_MACRO_WIFI_STATE_LIST
#undef X
};

/**
 * @brief WiFiApp State Machine Handler
 * @details
 * @param msg 
 */
static void wifiApp_stateMachine_handler(wifi_app_queue_message_t * msg)
{
	g_qtd_states = 0;
	#define X(ID, ENUM) g_qtd_states++; 
		X_MACRO_WIFI_STATE_LIST
	#undef X
	
	for (;;)
	{
		if (xQueueReceive(wifi_app_queue_handle_t, msg, portMAX_DELAY))
		{
			if(msg->wifiApp_state < g_qtd_states) {
				sm_wifi_state_table[msg->wifiApp_state].func(msg);
			}
		}
	}
}



/**************************
**	FreeRTOS FUNCTIONS	 **
**************************/
 
/**
 * @brief Main task for the WiFi application
 * where we initialize WiFi and and receive all queue messages
 * which determine the flow of the application
 * @details
 * @param pvParameters parameter which can be passed to the task
 */
static void wifiApp_task(void * pvParameters)
{
	wifi_app_queue_message_t msg;
	
	// Setup WiFi environment
	wifiApp_setup();
	
	// Start WiFi
	ESP_ERROR_CHECK(esp_wifi_start());
	
	// Send first event message
	wifiApp_sendMessage(WIFI_APP_LOAD_SAVED_CREDENTIALS);
	
	wifiApp_stateMachine_handler(&msg);
}

// Sends a message to the queue
// @param msgId message ID from the wifi_app_message_e enum
// @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE
/**
 * \todo colocar um enum status pra indicar falha ou sucesso
 */
BaseType_t wifiApp_sendMessage(sm_wifi_app_state_e msgId)
{
	wifi_app_queue_message_t msg;
	msg.wifiApp_state = msgId;
	return xQueueGenericSend(wifi_app_queue_handle_t, &msg, portMAX_DELAY, queueSEND_TO_BACK );
}

// Starts the WiFi RTOS task
void wifiApp_start(void)
{
	ESP_LOGI(TAG, "STARTING WIFI APPLICATION");
	
	// Start Wifi started LED
	// ledRGB_wifiApp_started();
	
	// Disable default WiFi logging messages
	esp_log_level_set("WiFi", ESP_LOG_NONE);
	
	// Clearing memory for the WiFi configuration
	memset(&wifi_config_v, 0x00, sizeof(wifi_config_t)); 
	
	// Create message queue
	wifi_app_queue_handle_t = xQueueCreate(3, sizeof(wifi_app_queue_message_t));

	// Create WiFI application event group
	wifi_app_event_group = xEventGroupCreate();
	
	// Start the WiFi application task
	CREATE_TASK(&wifiApp_task,
				"wifiApp_task",
				WIFI_APP_TASK_STACK_SIZE,
				NULL,
				WIFI_APP_TASK_PRIORITY,
#if defined BOARD_ESP32C6
				NULL);
#elif defined BOARD_ESP32S3
				NULL,
				WIFI_APP_TASK_CORE);
#endif
}



/**************************
**	 STA & AP FUNCTIONS	 **
**************************/

/**
 * @brief WiFi application event handler
 * @details
 * @param arg_p data, aside from event data, that is passed to the handler when it is called
 * @param event_base the base id of the event to register the handler for
 * @param eventId the id of the event to register the handler for
 * @param eventData_p event data
 */
 static void wifiApp_event_handler(void * arg_p, esp_event_base_t event_base, int32_t eventId, void * eventData_p)
 {
	 if (event_base == WIFI_EVENT)
	 {
		 switch (eventId)
		 {
			 case WIFI_EVENT_AP_START:
			 	ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
				// displayOled_printAccessPoint();
			 	break;
				
			 case WIFI_EVENT_AP_STOP:
			 	ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
			 	break;
				
			 case WIFI_EVENT_AP_STACONNECTED:
			 	ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
				// displayOled_printWebPageAccess();
			 	break;
				
			 case WIFI_EVENT_AP_STADISCONNECTED:
			 	ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
			 	break;
				
			 case WIFI_EVENT_AP_PROBEREQRECVED:
			 	ESP_LOGI(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
			 	break;
				
			 case WIFI_EVENT_STA_START:
			 	ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
			 	break;
				
			 case WIFI_EVENT_STA_STOP:
			 	ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
			 	break;
				
			 case WIFI_EVENT_STA_CONNECTED:
			 	ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
			 	break;
				
			 case WIFI_EVENT_STA_DISCONNECTED:
			 	ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
			 	wifiApp_sta_disconnectedLogInfo(eventData_p);
    
			    if( g_retry_number < MAX_CONNECTION_RETRIES)
			    {
					esp_wifi_connect();
					g_retry_number++;
				}
			    else
				{
					wifiApp_sendMessage(WIFI_APP_STA_DISCONNECTED);
				}
			 	break;
		 }
	 }
	 else if (event_base == IP_EVENT)
	 {
		 switch(eventId)
		 {
			 case IP_EVENT_STA_GOT_IP:
			 	ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
			 	
			 	wifiApp_sendMessage(WIFI_APP_STA_CONNECTED_GOT_IP);
			 	
			 	break;
		 }
	 }
 }

/**
 * @brief Initializes the WiFi application event handler for WiFi and IP events.
 * @details
 */
static void wifiApp_eventHandler_init(void)
{
	// Event loop for the WiFi driver
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	// Event handler for the connection
	esp_event_handler_instance_t instance_wifi_event;
	esp_event_handler_instance_t instance_ip_event;
	ESP_ERROR_CHECK(esp_event_handler_register( WIFI_EVENT,
												ESP_EVENT_ANY_ID,
												&wifiApp_event_handler,
												&instance_wifi_event));
	ESP_ERROR_CHECK(esp_event_handler_register( IP_EVENT,
												ESP_EVENT_ANY_ID,
												&wifiApp_event_handler,
												&instance_ip_event));
}


/**
 * @brief Initializes TCP stack and default WiFi configuration.
 * @details
 */
static void wifiApp_defaultWifi_init(void)
{
	// Initializes the TCP stack
	ESP_ERROR_CHECK(esp_netif_init());
	
	// Default Wifi config - operations must be in this order!
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}


static void wifiApp_configMode(wifi_mode_t wifi_mode)
{
	esp_netif_sta = esp_netif_create_default_wifi_sta();

	switch (wifi_mode)
	{
		case WIFI_MODE_APSTA:
			esp_netif_ap = esp_netif_create_default_wifi_ap();

			// SoftAP config
			wifiApp_softAP_config();
		break;


		case WIFI_MODE_STA:
			wifiApp_onlySTA_config();
		break;


		default:
			ESP_LOGE(TAG, "inconsistent wifi_mode on wifiApp_configMode funtion");
		break;
	}
}


static void wifiApp_onlySTA_config(void)
{
	// wifi_config_v.sta.listen_interval = WIFI_LISTENING_INTERVAL;
	wifi_config_v.sta.scan_method     =	WIFI_ALL_CHANNEL_SCAN;
	wifi_config_v.sta.sort_method     =	WIFI_CONNECT_AP_BY_SIGNAL;
	
	// Setting the mode as Station Mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_v));	///> set configuration

	wifi_config_v.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
	wifi_config_v.sta.failure_retry_cnt = 10;
}


/**
 * @brief Configures the WiFi access point settings and assigns the static IP to the SoftAP.
 * @details
 */
static void wifiApp_softAP_config(void)
{
	// SoftAP - WiFi access point configuration
	wifi_config_t ap_config =
	{
		.ap = {
			.ssid 			= 	WIFI_AP_SSID,
			.ssid_len 		=	strlen(WIFI_AP_SSID),
			// .password 		=	WIFI_AP_PASSWORD,
			.channel 		=	WIFI_AP_CHANNEL,
			.ssid_hidden	=	WIFI_AP_SSID_HIDDEN,
			.authmode		=	WIFI_AUTH_OPEN,
			.max_connection	=	WIFI_AP_MAX_CONNECTIONS,
			.beacon_interval=	WIFI_AP_BEACON_INTERVAL,
		},
	};
	
	// Stopping the DHCP server before updating it
	esp_netif_dhcps_stop(esp_netif_ap);
	
	// Configure DHCP for the AP
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	
	// Assign access point's static IP, GW and Netmask
	//(converts string, to the wanted binary form)
	inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);
	
	// Statically configure the network interface
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	// Start the AP DHCP server (for connecting stations e.g. your mobile device)
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
	
	// Setting the mode as Access Point / Station Mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));	///> set configuration
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));	///> default bandwidth 20 MHz
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));						///> power save set to "none"
}

/**
 * @brief Connects the ESP32 to an external AP using the updated station configuration
 * @details
 */
static void wifiApp_sta_connect(void)
{
	wifiApp_setRealCredentialsToConnect();
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_v));
	ESP_ERROR_CHECK(esp_wifi_connect());
}

/**
 * @brief A function to log at the serial monitor the reasons of why the station disconnected
 * @details
 * @param eventData_p 
 */
static void wifiApp_sta_disconnectedLogInfo(void * eventData_p)
{
	wifi_event_sta_disconnected_t *wifi_event = (wifi_event_sta_disconnected_t *)eventData_p;
    ESP_LOGI(TAG, "WiFi disconnected, reason: %d", wifi_event->reason);
}



/**************************
**		  MEMORY		 **
**************************/

static esp_err_t wifiApp_loadCredentials(void)
{
	return hal_memory_load_blob(&wifi_credentials_memorizer);
}


static void wifiApp_saveCredentials(void)
{
	hal_memory_save_blob(&wifi_credentials_memorizer);
}


static void wifiApp_forgetCredentials(void)
{
	hal_memory_erase_key(&wifi_credentials_memorizer);
}