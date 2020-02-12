#ifndef __WEB_FILE_SERVER_H__
#define __WEB_FILE_SERVER_H__

#include "esp_http_server.h"

typedef enum
{
    WEBFILESERVER_CMD_STAT = 0,
    WEBFILESERVER_CMD_REQ,
    WEBFILESERVER_CMD_DL,
    WEBFILESERVER_CMD_RESET,
    WEBFILESERVER_CMD_LIST,
    WEBFILESERVER_NO_CMD = 99,
} web_file_server_cmd_t;
#define WEBFILESERVER_NUM_ENTRIES   5

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

/* Example view of the web page:

<h1 style="text-align: center;">Beacon List</h1>
<table style="margin-left: auto; margin-right: auto;" border="0" width="600" bgcolor="#e0e0e0">
<tbody>
<tr bgcolor="#c0c0c0">
<td style="text-align: center;">Name</td>
<td style="text-align: center;">Last seen</td>
<td style="text-align: center;">Status</td>
<td style="text-align: center;">Command</td>
<td style="text-align: center;">Download</td>
</tr>
<tr>
<td>Bx0706</td>
<td>08.02.2020 07:03</td>
<td>done</td>
<td><a href="/csv?beac=Bx0706&amp;cmd=reset">reset</a></td>
<td><a href="/csv?beac=Bx0706&amp;cmd=dl">download</a></td>
</tr>
<tr>
<td>Bx0708</td>
<td>08.02.2020 07:03</td>
<td>queued</td>
<td><a href="/csv?beac=Bx0706&amp;cmd=reset">reset</a></td>
<td>&nbsp;</td>
</tr>
<tr>
<td>Bx0709</td>
<td>08.02.2020 07:03</td>
<td>available</td>
<td><a href="/csv?beac=Bx0706&amp;cmd=prep">prepare</a></td>
<td>&nbsp;</td>
</tr>
</tbody>
</table>

*/

#endif // __WEB_FILE_SERVER_H__