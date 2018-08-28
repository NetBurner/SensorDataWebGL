/* Revision: 2.8.7 */

/******************************************************************************
* Copyright 1998-2018 NetBurner, Inc.  ALL RIGHTS RESERVED
*
*    Permission is hereby granted to purchasers of NetBurner Hardware to use or
*    modify this computer program for any use as long as the resultant program
*    is only executed on NetBurner provided hardware.
*
*    No other rights to use this program or its derivatives in part or in
*    whole are granted.
*
*    It may be possible to license this or other NetBurner software for use on
*    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
*
*    NetBurner makes no representation or warranties with respect to the
*    performance of this computer program, and specifically disclaims any
*    responsibility for any damages, special or consequential, connected with
*    the use of this program.
*
* NetBurner
* 5405 Morehouse Dr.
* San Diego, CA 92121
* www.netburner.com
******************************************************************************/

/**
 * Handles and implements high level web server functionality.
 */

#include <effs_fat/fat.h>
#include <http.h>
#include <iosys.h>
#include <websockets.h>

#include "cardtype.h"

#define HTTP_BUFFER_SIZE (32 * 1024)   // Make a 32KB BUFFER
static char HTTP_buffer[HTTP_BUFFER_SIZE] __attribute__((aligned(16)));

static http_gethandler *oldhand = nullptr;
extern http_wshandler *TheWSHandler = nullptr;

int ws_fd = -1;

/**
 * @brief Send a fragment of a file over a socket
 */
void SendFragment(int sock, F_FILE *f, long len)
{
    int lread = 0;
    while (lread < len)
    {
        int ltoread = len - lread;
        int lr;

        if (ltoread > HTTP_BUFFER_SIZE) { ltoread = HTTP_BUFFER_SIZE; }

        lr = f_read(HTTP_buffer, 1, HTTP_BUFFER_SIZE, f);

        if (lr == 0) { return; }

        lread += lr;
        writeall(sock, HTTP_buffer, lr);
    }
}

/**
 * @brief Displays a list of directories and files to the web browser
 */
void WebListDir(int sock, const char *dir)
{
    writestring(sock, "HTTP/1.0 200 OK\r\n");
    writestring(sock, "Pragma: no-cache\r\n");
    writestring(sock, "MIME-version: 1.0\r\n");
    writestring(sock, "Content-Type: text/html\r\n\r\n");
    writestring(sock, "<html>\r\n");
    writestring(sock, "   <body>\r\n");
    writestring(sock, "      <h2><font face=\"Arial\">Directory of ");
    writestring(sock, dir);
    writestring(sock, "</font></h2>\r\n");
    writestring(sock, "      <hr>\r\n");
    writestring(sock, "      <ul><font face=\"Courier New\" size=\"2\">\r\n");

    F_FIND f;
    int rc = f_findfirst("*.*", &f);

    while (rc == 0)
    {
        if (f.attr & F_ATTR_DIR)
        {
            writestring(sock, "         <li><img src=\"/folder.gif\"><a href=\"");
            writestring(sock, f.filename);
            writestring(sock, "/DIR\">");
            writestring(sock, f.filename);
            writestring(sock, "</a>\r\n");
        }
        else
        {
            writestring(sock, "         <li><img src=\"/text.gif\"><a href=\"");
            writestring(sock, f.filename);
            writestring(sock, "\">");
            writestring(sock, f.filename);
            writestring(sock, "</a>\r\n");
        }

        rc = f_findnext(&f);
    }

    writestring(sock, "      </font></ul>\r\n");
    writestring(sock, "      <hr>\r\n");
    writestring(sock, "   </body>\r\n");
    writestring(sock, "</html>");
}

#define tmp_buffer_size (256)
char tmp_buffer[tmp_buffer_size];
int tmp_buffer_end;
int tmp_buffer_start;

/**
 * @brief Reads and returns 1 line from the file FP.
 */

int my_f_read_line(char *buffer, int buf_siz, F_FILE *fp)
{
    int nr = 0;
    do
    {
        if (tmp_buffer_end <= tmp_buffer_start)
        {
            if (f_eof(fp)) return 0;

            int n = f_read(tmp_buffer, 1, tmp_buffer_size, fp);
            tmp_buffer_start = 0;
            tmp_buffer_end = n;

            if (n == 0)
            {
                buffer[nr + 1] = 0;
                return nr;
            }
        }

        *(buffer + nr) = tmp_buffer[tmp_buffer_start++];

        if ((buffer[nr] == '\r') || (buffer[nr] == '\n'))
        {
            if (nr != 0)
            {
                buffer[nr + 1] = 0;
                return nr;
            }
        }
        else
        {
            nr++;
        }
    } while (nr < buf_siz);

    buffer[nr + 1] = 0;
    return nr;
}

/**
 * @brief Takes a file type and sends a header response with the specified MIME type.
 *
 * Returns: number of bytes written to socket.
 *
 * Normally the netburner http code will send the proper header response based on the MIME_magic.txt
 * file located in \nburn\pcbin. However, if the file is read from the flash card, the http server code
 * does not know about the type.
 *
 * This example function adds support for a subset of common file types in 2 ways. First, it tries to find
 * a file called MIME.txt on the SD card. This file looks like the following:
 *     -------MIME.txt--------
 *     jpg     image/jpeg
 *     html    text/html
 *     ...
 *     xml     text/xml
 *
 * Note that MIME types are not cached, and numerous lookups on a large MIME.txt file can be costly.
 * Only include the minimum number of file types you wish to support. Order matters as well, so
 * common file types should be located at the top
 *
 * If the file type is located, then the specified MIME type is sent.
 *
 * If the MIME lookup fails, a secondary MIME lookup occurs with hard-coded values.
 */

int SendEFFSCustomHeaderResponse(int sock, char *fType)
{
    char mime_type[64];
    bool found = false;

    // Check for MIME.txt file, which lists support mime types
    F_FILE *f = f_open("MIME.txt", "r");
    if (f != nullptr)
    {
        char line[255] = "";
        while (my_f_read_line(line, 255, f) != 0 && !found)
        {
            if (line[0] == '#' || line[0] == ' ' || line[0] == '\0')
            {
                continue;   // Comment
            }
            char *pch = strtok(line, " \t\n\r");
            if (strcasecmp(fType, pch) == 0)
            {   // Found file type
                pch = strtok(nullptr, " \t\n\r");
                sniprintf(mime_type, 64, pch);
                found = true;
            }
        }
    }
    if (!found)
    {                   // no MIME.txt found or extension type not found, fall back to hard-coded list
        found = true;   // Set to true. Revert to false if not found in default else.
        if (strcasecmp(fType, "jpg") == 0)
        {
            sniprintf(mime_type, 64, "image/jpeg");
        }
        else if (strcasecmp(fType, "gif") == 0)
        {
            sniprintf(mime_type, 64, "image/gif");
        }
        else if (strcasecmp(fType, "htm") == 0)
        {
            sniprintf(mime_type, 64, "text/html");
        }
        else if (strcasecmp(fType, "html") == 0)
        {
            sniprintf(mime_type, 64, "text/html");
        }
        else if (strcasecmp(fType, "xml") == 0)
        {
            sniprintf(mime_type, 64, "text/xml");
        }
        else if (strcasecmp(fType, "css") == 0)
        {
            sniprintf(mime_type, 64, "text/css");
        }
        else if (strcasecmp(fType, "mp4") == 0)
        {
            sniprintf(mime_type, 64, "video/mp4");
        }
        else
        {
            found = false;
        }
    }
    char buffer[255];
    if (found)
    {
        sniprintf(buffer, 255,
                  "HTTP/1.0 200 OK\r\n"
                  "Pragma: no-cache\r\n"
                  "MIME-version: 1.0\r\n"
                  "Content-Type: %s\r\n\r\n",
                  mime_type);
    }
    else
    {   // If MIME type is not found, don't send any MIME type. This allows the browser to
        // make a best guess
        sniprintf(buffer, 255,
                  "HTTP/1.0 200 OK\r\n"
                  "Pragma: no-cache\r\n\r\n");
    }
    int bytes = writestring(sock, buffer);
    return bytes;
}

/**
 * @brief Handles a WebSocket upgrade request
 */
int MyDoWSUpgrade(HTTP_Request *req, int sock, PSTR url, PSTR rxBuffer)
{
    iprintf("Trying WebSocket Upgrade!\r\n");
    if (httpstricmp(url, "INDEX"))
    {
        if (ws_fd > 0)
        {
            iprintf("Closing prior WebSocket connection.\r\n");
            close(ws_fd);
            ws_fd = -1;
        }

        int rv = WSUpgrade(req, sock);
        if (rv >= 0)
        {
            iprintf("WebSocket Upgrade Successful!\r\n");
            ws_fd = rv;
            NB::WebSocket::ws_setoption(ws_fd, WS_SO_TEXT);
            return 2;
        }
        else
        {
            return 0;
        }
    }

    NotFoundResponse(sock, url);
    return 0;
}

/**
 * @brief Handles our GET requests
 */
int MyDoGet(int sock, PSTR url, PSTR rxBuffer)
{
    char name_buffer[257] = {};   // Reserve last byte for null character
    char dir_buffer[256] = {};
    char ext_buffer[10] = {};

#ifdef USE_MMC
    f_chdrive(MMC_DRV_NUM);
#endif

#ifdef USE_CFC
    f_chdrive(CFC_DRV_NUM);
#endif

    f_chdir("\\");

    iprintf("Processing MyDoGet()\r\n");

    // Parse and store file extension portion of URL
    iprintf("  URL: \"%s\"\r\n", url);
    char *pext = url + strlen(url);

    while ((*pext != '.') && (*pext != '/') && (*pext != '\\') && (pext > url))
    {
        pext--;
    }

    if ((*pext == '.') || (*pext == '\\') || (*pext == '/')) { pext++; }

    strncpy(ext_buffer, pext, 9);
    iprintf("  URL extension: \"%s\"\r\n", ext_buffer);

    // Parse and store file name portion of URL
    char *pName = url + strlen(url);

    while ((*pName != '/') && (*pName != '\\') && (pName > url))
    {
        pName--;
    }

    if ((*pName == '\\') || (*pName == '/')) { pName++; }

    strncpy(name_buffer, pName, 256);
    iprintf("  URL file name: \"%s\"\r\n", name_buffer);

    // Store directory portion of URL
    strncpy(dir_buffer + 1, url, (pName - url));
    dir_buffer[0] = '/';
    dir_buffer[(pName - url) + 1] = 0;
    iprintf("  URL directory portion: \"%s\"\r\n", dir_buffer);

    /**
     * Try to locate the specified file on the flash card. If no file
     * name is given, then search for a html file in the following order:
     * 
     *     1. index.htm or index.html
     *     2. any file ending in .htm
     *     3. any file ending in .html
     * 
     * In this way any HTML file on the flash card will override the HTML
     * files on the module internal flash memory
     */ 
    if (f_chdir(dir_buffer) == F_NO_ERROR)
    {
        if (name_buffer[0] == 0)
        {
            if (dir_buffer[1] == 0)
            {
                // Root file try index.ht* first
                F_FIND f;
                int rc = f_findfirst("index.ht*", &f);

                if (rc != 0)
                {
                    rc = f_findfirst("*.htm", &f);
                    if (rc != 0) rc = f_findfirst("*.html", &f);
                }

                if (rc == 0)
                {
                    RedirectResponse(sock, f.filename);
                    return 0;
                }
            }

            /**
             * The default behavior of this application is to display the index.htm
             * file in the compiled application image located in the flash chip on
             * the NetBurner module. If you would like to change the behavior so that
             * a directory listing is displayed instead of the internal index.htm,
             * then uncomment the following two lines of code
             */
             // WebListDir(sock, dir_buffer);
             // return 0;
        }
        else
        {
            // A file name was specified in the URL, so attempt to open it
            iprintf("  Attempting to open file \"%s\"...", pName);
            F_FILE *f = f_open(pName, "r");

            if (f != nullptr)
            {
                long len = f_filelength(pName);
                SendEFFSCustomHeaderResponse(sock, ext_buffer);
                SendFragment(sock, f, len);
                f_close(f);
                iprintf(" File sent to browser\r\n");
                return 0;
            }
            else
            {
                iprintf(" file does not exist on flash card,");
                iprintf(" will look in compiled application image\r\n");
            }

            /**
             * If the work "DIR" is specified in the URL at any directory level,
             * then this code will result in a directory listing
             */
            if (httpstricmp(pName, "DIR"))
            {
                WebListDir(sock, dir_buffer);
                return 0;
            }
        }
    }

    return (*oldhand)(sock, url, rxBuffer);
}

/**
 * @brief Registers the callbacks we want to use for our GET and WebSocket upgrade requests
 */
void RegisterWebFuncs()
{
    oldhand = SetNewGetHandler(MyDoGet);
    TheWSHandler = MyDoWSUpgrade;
}
