#ifndef __WEB_FILE_SERVER_H__
#define __WEB_FILE_SERVER_H__

#include "esp_http_server.h"

typedef enum
{
    WEBFILESERVER_CMD_STAT = 0,
    WEBFILESERVER_CMD_PREP,
    WEBFILESERVER_CMD_DL,
    WEBFILESERVER_CMD_RESET,
    WEBFILESERVER_CMD_LIST,
    WEBFILESERVER_NUM_ENTRIES,
    WEBFILESERVER_NO_CMD = 99
} web_file_server_cmd_t;

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);


#endif // __WEB_FILE_SERVER_H__