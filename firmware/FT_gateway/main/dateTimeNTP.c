/**
 * @file dateTimeNTP.c
 * @author your name (you@domain.com)
 * @brief
 * @details
 * @version 0.1
 * @date 2025-05-21
 * 
 * @copyright Public Domain (or CC0 licensed, at your option.) 2025
 * 
 */


/**************************
**		  INCLUDES	 	 **
**************************/

// C libraries
#include <stdio.h>          // for printf
#include <string.h>         // for string manipulation
#include <errno.h>

// ESP libraries
#include "sys/socket.h"     // for socket
#include "netdb.h"          // for gethostnameby
#include "unistd.h"         // for closing sockets
#include "esp_log.h"

// Personal libraries
#include "dateTimeNTP.h"
#include "tasks_common.h"
#include "wifiApp.h"



/**************************
**		DECLARATIONS	 **
**************************/

	/* Variables */

/**
 * @brief Tag used for ESP serial console messages
 * 
 */
static const char TAG[] = "NTP-date-time";

/**
 * @brief Casting the old structure used in C (sockaddr_in) to this new struct used currently on web
 * 
 */
typedef struct sockaddr SA;

/**
 * @brief Default common value to attend every ip
 * 
 */
static const int ip_protocol = 0;

/**
 * @brief Variables to store Date and Time from NTP server
 * 
 */
char date_str[DATE_LEN]={0}, time_str[TIME_LEN]={0};


/* Static Functions */

/**
 * @brief Event callback function
 * 
 */
static void dateTimeNTP_wifiApp_connectedEvents(void);

/**
 * @brief Callback function for NTP date and time queries
 * 
 */
static void dateTimeNTP_update_task(void *pvParameter);

/**
 * @brief function that fetches datetime from pool.ntp.org
 * 
 */
static void ntp_fetchData(void);



/**************************
**	   APP FUNCTIONS 	 **
**************************/
char* dateTimeNTP_getTime(void)
{
    return time_str;
}

char* dateTimeNTP_getData(void)
{
    return date_str;
}

void dateTimeNTP_setup(void)
{
	// Set the wifi connected event callback function
	wifiApp_setCallbacks(
        NULL,
        dateTimeNTP_wifiApp_connectedEvents
    );
}

static void dateTimeNTP_wifiApp_connectedEvents(void)
{
	ESP_LOGI(TAG, "WiFi Application Connected!");

	// Start the fetch dateTime Task
	xTaskCreatePinnedToCore(	&dateTimeNTP_update_task,
								"router_fetchDateTime",
								NTP_DATE_TIME_TASK_STACK_SIZE,
								NULL,
								NTP_DATE_TIME_TASK_PRIORITY,
								NULL,
								NTP_DATE_TIME_TASK_CORE_ID);
}

static void dateTimeNTP_update_task(void *pvParameter)
{
    for(;;)
	{
        ntp_fetchData();
		// displayOled_printDateTime(date_str, time_str);
        vTaskDelay(30000 / portTICK_PERIOD_MS); // Sync every 30 seconds
    }
}



/**************************
**	  SOCKET FUNCTIONS 	 **
**************************/

// function that fetches datetime from pool.ntp.org
static void ntp_fetchData(void)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    ntp_packet packet;
    int n;
    time_t txTm;
    struct tm *local_time;

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&packet, 0, sizeof(ntp_packet));


    // Create a UDP socket
    sockfd = socket(AF_INET, UDP_SOCKET, ip_protocol);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "ERROR opening socket");
        return;
    }

    // Get server address information
    server = gethostbyname(NTP_SERVER);
    if (server == NULL) {
        ESP_LOGE(TAG, "ERROR, no such host\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(NTP_PORT);

    // Initialize NTP packet
    packet.li_vn_mode = 0x1B; // li = 0, vn = 3, mode = 3

    // Send NTP request
    n = sendto(sockfd, &packet, sizeof(ntp_packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (n < 0) {
        ESP_LOGE(TAG, "ERROR writing to socket");
        return;
    }

    // Receive NTP response
    n = recvfrom(sockfd, &packet, sizeof(ntp_packet), 0, NULL, NULL);
    if (n < 0) {
        ESP_LOGE(TAG, "ERROR reading from socket");
        return;
    }

    // Parse NTP response
    packet.txTm_s = ntohl(packet.txTm_s);
    packet.txTm_f = ntohl(packet.txTm_f);

    // Calculate Unix timestamp
    txTm = packet.txTm_s - NTP_TIMESTAMP_DELTA;

    // Set timezone (example for UTC)
    setenv("TZ", "UTC+3", 1);
    tzset(); 

    // Convert Unix timestamp to local time
    local_time = localtime(&txTm);

    // Format date and time separately
    memset(date_str, 0, DATE_LEN);
    strftime(date_str, sizeof(date_str), "%d/%m/%Y", local_time);
    memset(time_str, 0, TIME_LEN);
    strftime(time_str, sizeof(time_str), "%H:%M", local_time);

    // Print the date and time
    printf("Date: %s\n", date_str);
    printf("Time: %s\n", time_str);

    close(sockfd);
}