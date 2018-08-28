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
* Handles and implements high level FTP server API calls.
*/

// NB Libs
#include <startnet.h>
#include <tcp.h>

// NB FTP
#include <ftpd.h>

#ifdef USE_MMC
#include <effs_fat/mmc_mcf.h>
#endif
#ifdef USE_CFC
#include <effs_fat/cfc_mcf.h>
#endif

#include "cardtype.h"
#include "FileSystemUtils.h"
#include "ftp_f.h"

#define LOGME iprintf("We made it to line %d of file %s.\r\n", __LINE__, __FILE__);

#define FTP_BUFFER_SIZE (32 * 1024)

static int FileSysRetryLimit = 10;
static int NetworkRetryLimit = 10;

static char FTP_buffer[FTP_BUFFER_SIZE] __attribute__((aligned(16)));
static const char mstr[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief Set the date.
 */
static void settimedate(F_FIND *f)
{
    int nret = f_settimedate(f->filename, f_gettime(), f_getdate());
    if (nret == F_NO_ERROR)
    {
        // iprintf( "Time stamping successful\r\n" );
    }
    else
    {
        iprintf("Time stamping failed: %d\r\n", nret);
    }
}

/**
 * @brief This function displays information in MTTTY of subdirectories
 * and files in a current directory.  The information displayed is: the
 * filename, modified time stamp, modified date stamp, and file size.
 */
static void gettimedate(F_FIND *f)
{
    unsigned short t, d;

    int nret = f_gettimedate(f->filename, &t, &d);
    if (nret == F_NO_ERROR)
    {
        iprintf("%15s   |", f->filename);
        iprintf("%2.2d:%2.2d:%2.2d   |", ((t & 0xF800) >> 11), ((t & 0x07E0) >> 5), 2 * (t & 0x001F));
        iprintf("%2.2d/%2.2d/%4d   |", ((d & 0x01E0) >> 5), (d & 0x001F), (1980 + ((d & 0xFE00) >> 9)));
        iprintf("%9ld Bytes\r\n", f->filesize);
    }
    else
    {
        iprintf("Time stamp retrieval failed: %d\r\n", nret);
    }
}

/**
 * @brief Generate date string.
 */
static void getdatestring(F_FIND *f, char *tmp)
{
    // Converts file time and date stamp to a value that can be interpreted by the user.
    // unsigned short sec = 2 * ( ( f->ctime ) & 0x1F );
    unsigned short minute = ((f->ctime) & 0x7E0) >> 5;
    unsigned short hour = ((f->ctime) & 0xF800) >> 11;
    unsigned short day = (f->cdate) & 0x1F;
    unsigned short month = ((f->cdate) & 0x01E0) >> 5;
    unsigned short year = 1980 + (((f->cdate) & 0xFE00) >> 9);

    // For FTP file properties: If the current year matches the year stamp of the
    // associated file, then the hour and minutes are displayed.  Otherwise, the
    // year is used in place of hour and minutes.
    if ((1980 + (((f_getdate()) & 0xFE00) >> 9)) == year)
    {
        siprintf(tmp, "%3s %2d %2.2d:%2.2d", mstr[month - 1], day, hour, minute);
    }
    else
    {
        siprintf(tmp, "%3s %2d  %4d", mstr[month - 1], day, year);
    }
}

/**
 * @brief Generate directory entry string.
 */
static void getdirstring(F_FIND *f, char *dst)
{
    char tmp[256];

    if ((f->attr) & F_ATTR_DIR)
    {
        dst[0] = 'd';
    }
    else
    {
        dst[0] = '-';
    }

    dst[1] = 0;
    strcat(dst, "-rw-rw-rw-   1 none");
    strcat(dst, " ");

    sniprintf(tmp, 256, "%9ld", f->filesize);
    strcat(dst, tmp);
    strcat(dst, " ");

    getdatestring(f, tmp);
    strcat(dst, tmp);
    strcat(dst, " ");

    strcat(dst, f->filename);
}

/**
 * @brief Start the FTP session.
 */
void *FTPDSessionStart(const char *user, const char *passwd, const IPADDR4 hi_ip)
{
    iprintf(" Starting FTP session\r\n");

#ifdef USE_MMC
    f_chdrive(MMC_DRV_NUM);
#endif /* USE_MMC */

#ifdef USE_CFC
    f_chdrive(CFC_DRV_NUM);
#endif /* USE_CFC */

    f_chdir("\\");

    return ((void *)1);
}

/**
 * @brief Finish the FTP session.
 */
void FTPDSessionEnd(void *pSession) {}

/**
 * @brief Check for a directory.
 */
int FTPD_DirectoryExists(const char *full_directory, void *pSession)
{
    if (*full_directory == 0) { return (FTPD_OK); }

    f_chdir("\\");

    if (f_chdir((char *)full_directory))
    {
        return (FTPD_FAIL);
    }
    else
    {
        return (FTPD_OK);
    }

    return (FTPD_OK);
}

/**
 * @brief Create a directory.
 */
int FTPD_CreateSubDirectory(const char *current_directory, const char *new_dir, void *pSession)
{
    int rc;

    f_chdir("/");
    if (*current_directory)
    {
        rc = f_chdir((char *)current_directory);
        if (rc) { return (FTPD_FAIL); }
    }

    rc = f_mkdir((char *)new_dir);
    if (rc == 0) { return (FTPD_OK); }

    return (FTPD_FAIL);
}

/**
 * @brief Delete a directory.
 */
int FTPD_DeleteSubDirectory(const char *current_directory, const char *sub_dir, void *pSession)
{
    int rc;

    f_chdir("/");

    if (*current_directory)
    {
        rc = f_chdir((char *)current_directory);
        if (rc) { return (FTPD_FAIL); }
    }

    rc = f_rmdir((char *)sub_dir);

    if (rc == 0) { return (FTPD_OK); }

    return (FTPD_FAIL);
}

/**
 * @brief List directories.
 */
int FTPD_ListSubDirectories(const char *current_directory, void *pSession, FTPDCallBackReportFunct *pFunc, int socket)
{
    F_FIND find;
    long rc;
    char s[256];

    f_chdir("/");

    if (*current_directory)
    {
        if (f_chdir((char *)current_directory)) { return (FTPD_FAIL); }
    }

    rc = f_findfirst("*.*", &find);
    if (!rc)
    {
        do
        {
            if (find.attr & F_ATTR_DIR)
            {
                getdirstring(&find, s);
                pFunc(socket, s);
            }
        } while (!f_findnext(&find));
    }

    return (FTPD_OK);
}

/**
 * @brief Check for a file.
 */
int FTPD_FileExists(const char *full_directory, const char *file_name, void *pSession)
{
    F_FILE *t;

    if (strcmp(file_name, "_format") == 0 || strcmp(file_name, "_hformat") == 0
#ifdef USE_CFC
        || strcmp(file_name, "_cfc") == 0
#endif /* USE_CFC */

#ifdef USE_HDD
        || strcmp(file_name, "_hdd") == 0
#endif /* USE_HDD */

#ifdef USE_MMC
        || strcmp(file_name, "_mmc") == 0
#endif /* USE_MMC */

#ifdef USE_FATRAM
        || strcmp(file_name, "_fram") == 0
#endif /* USE_FATRAM */

#ifdef USE_NOR
        || strcmp(file_name, "_nor") == 0
#endif /* USE_NOR */

#ifdef USE_STDRAM
        || strcmp(file_name, "_sram") == 0
#endif /* USE_STDRAM */
    )
    {
        return (FTPD_OK);
    }

    f_chdir("/");

    if (*full_directory)
    {
        if (f_chdir((char *)full_directory)) { return (FTPD_FAIL); }
    }

    t = f_open((char *)file_name, "r");

    if (t)
    {
        f_close(t);
        return (FTPD_OK);
    }

    return (FTPD_FAIL);
}

/**
 * @brief Get a file's size.
 */
int FTPD_GetFileSize(const char *full_directory, const char *file_name)
{
    F_STAT stat;
    f_chdir("/");

    uint32_t len = strlen(file_name);
    if ((file_name[len - 1] != '/') && (FTPD_FileExists(full_directory, file_name, nullptr) == FTPD_FAIL))
    {
        return FTPD_FILE_SIZE_NOSUCH_FILE;
    }

    if (*full_directory)
    {
        if (f_chdir((char *)full_directory)) { return FTPD_FILE_SIZE_NOSUCH_FILE; }
    }

    if (file_name[len - 1] == '/')
    {
        stat.filesize = 0;
    }
    else if (f_stat(file_name, &stat))
    {
        return FTPD_FILE_SIZE_NOSUCH_FILE;
    }

    return stat.filesize;
}

/**
 * @brief Retrieve function for FTP (get).
 */
int FTPD_SendFileToClient(const char *full_directory, const char *file_name, void *pSession, int fd)
{
    F_FILE *rfile;   // Actual opened file

    int TotalBytesWritten = 0, BytesWritten = 0, BytesRead = 0, RetryAttempts = 0, TransferError = 0;

#ifdef USE_NOR
    if (strcmp(file_name, "_nor") == 0)
    {
        // iprintf( "Change to NOR.\r\n" );
        f_chdrive(NOR_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_NOR */

#ifdef USE_STDRAM
    if (strcmp(file_name, "_sram") == 0)
    {
        // iprintf( "Change to STDRAM.\r\n" );
        f_chdrive(STDRAM_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_STDRAM */

#ifdef USE_MMC
    if (strcmp(file_name, "_mmc") == 0)
    {
        // iprintf( "Change to MMC.\r\n" );
        f_chdrive(MMC_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_MMC */

#ifdef USE_CFC
    if (strcmp(file_name, "_cfc") == 0)
    {
        // iprintf( "Change to CFC.\r\n" );
        f_chdrive(CFC_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_CFC */

#ifdef USE_HDD
    if (strcmp(file_name, "_hdd") == 0)
    {
        // iprintf( "Change to HDD.\r\n" );
        f_chdrive(HDD_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_HDD */

#ifdef USE_FATRAM
    if (strcmp(file_name, "_fram") == 0)
    {
        // iprintf( "Change to FATRAM.\r\n" );
        f_chdrive(FATRAM_DRV_NUM);
        return (FTPD_FAIL);
    }
#endif /* USE_FATRAM */

    if (strcmp(file_name, "_format") == 0)
    {
        // iprintf( "Formatting...\r\n" );
        f_format(f_getdrive(), F_FAT32_MEDIA);
        return (FTPD_FAIL);
    }

    f_chdir("/");

    if (*full_directory)
    {
        if (f_chdir((char *)full_directory)) { return (FTPD_FAIL); }
    }

    /* If file is not open, then try to open it. */
    rfile = f_open((char *)file_name, "r");

    /* Return with error if not successful. */
    if (!rfile) { return (FTPD_FAIL); }

    SetSocketTxBuffers(fd, 20);

    while (!f_eof(rfile))
    {
        BytesRead = 0;
        RetryAttempts = 0;

        while ((RetryAttempts < FileSysRetryLimit) && (BytesRead == 0))
        {
            BytesRead = f_read(FTP_buffer, 1, FTP_BUFFER_SIZE, rfile);   // Read from file

            // retry if busy
            if ((BytesRead == 0) && (!f_eof(rfile)))
            {
                int error = f_getlasterror();
                if (error) { DisplayEffsErrorCode(error); }
                OSTimeDly(TICKS_PER_SECOND / 4);
                RetryAttempts++;
            }
        }

        if (RetryAttempts >= FileSysRetryLimit)
        {
            TransferError = 1;
            break;
        }

        BytesWritten = 0;
        TotalBytesWritten = 0;
        RetryAttempts = 0;

        while ((RetryAttempts < NetworkRetryLimit) && (TotalBytesWritten != BytesRead))
        {
            BytesWritten = write(fd, FTP_buffer + TotalBytesWritten, BytesRead - TotalBytesWritten);

            // retry if busy
            if (BytesWritten <= 0)
            {
                RetryAttempts++;
                OSTimeDly(TICKS_PER_SECOND / 4);
            }

            TotalBytesWritten += BytesWritten;
        }

        if (RetryAttempts >= NetworkRetryLimit) { TransferError = 2; }
    }

    f_close(rfile);   // Close the file

    if (TransferError)
    {
        if (TransferError == 1)
        {
            iprintf("There was an error reading %s/%s from file system\r\n", full_directory, file_name);
        }
        else if (TransferError == 2)
        {
            iprintf("There was an error writing %s/%s to the network\r\n", full_directory, file_name);
        }

        return (FTPD_FAIL);
    }
    else
    {
        return (FTPD_OK);
    }
}

/**
 * @brief Check if it is possible to create a file.
 */
int FTPD_AbleToCreateFile(const char *full_directory, const char *file_name, void *pSession)
{
    return (FTPD_OK);
}

/**
 * @brief Write file to drive.
 */
int FTPD_GetFileFromClient(const char *full_directory, const char *file_name, void *pSession, int fd)
{
    F_FILE *wfile = 0;   // Actual opened file
    F_FIND find;
    int BytesWritten = 0, LastBytesWritten = 0, BytesRead = 0, RetryAttempts = 0, TransferError = 0;

    f_chdir("/");

    if (*full_directory)
    {
        if (f_chdir((char *)full_directory)) { return (FTPD_FAIL); }
    }

    /* Only accept file names of size 12 (8+1+3). */
    // if ( strlen( ( char * ) file_name ) > 12 ) return( FTPD_FAIL );

    wfile = f_open((char *)file_name, "w");   // Open it for write

    int rc = f_findfirst(file_name, &find);
    if (rc == 0)
    {
        if (!(find.attr & F_ATTR_DIR)) { settimedate(&find); }
    }
    else
    {
        iprintf("f_findfirst failed\r\n");
    }

    /* Return error if not successful. */
    if (!wfile) { return (FTPD_FAIL); }

    SetSocketRxBuffers(fd, 20);

    while (1)
    {
        // Read file data from the FTP data socket

        RetryAttempts = 0;
        BytesRead = 0;

        while ((BytesRead == 0) && (RetryAttempts < NetworkRetryLimit))
        {
            BytesRead = ReadWithTimeout(fd, FTP_buffer, FTP_BUFFER_SIZE, TICKS_PER_SECOND);
            RetryAttempts++;
        }

        if ((RetryAttempts >= NetworkRetryLimit) && (BytesRead == 0)) { TransferError = 2; }

        if (BytesRead < 1) { break; }

        // Now write the file data to the file system

        BytesWritten = 0;
        LastBytesWritten = 0;
        RetryAttempts = 0;

        while ((RetryAttempts < FileSysRetryLimit) && (BytesWritten < BytesRead))
        {
            BytesWritten += f_write(FTP_buffer + BytesWritten, 1, BytesRead - BytesWritten, wfile);
            if (BytesWritten == LastBytesWritten)
            {
                int error = f_getlasterror();
                if (error) DisplayEffsErrorCode(error);
                OSTimeDly(TICKS_PER_SECOND / 4);
                RetryAttempts++;
                // iprintf("Retry for write = %d\r\n", RetryAttempts);
            }
            LastBytesWritten = BytesWritten;
        }

        if (RetryAttempts >= FileSysRetryLimit) { TransferError = 1; }

        if (BytesRead < 1) { break; }
    }

    f_close(wfile);

    if (TransferError)
    {
        if (TransferError == 1)
        {
            iprintf("There was an error writing %s/%s to file system\r\n", full_directory, file_name);
        }
        else if (TransferError == 2)
        {
            iprintf("There was an error reading %s/%s from the network\r\n", full_directory, file_name);
        }

        return (FTPD_FAIL);
    }
    else
    {
        return (FTPD_OK);
    }
}

/**
 * @brief Delete a file.
 */
int FTPD_DeleteFile(const char *current_directory, const char *file_name, void *pSession)
{
    f_chdir("/");

    if (*current_directory)
    {
        if (f_chdir((char *)current_directory)) { return (FTPD_FAIL); }
    }

    if (f_delete((char *)file_name)) { return (FTPD_FAIL); }

    return (FTPD_OK);
}

/**
 * @brief Get a file list.
 */
int FTPD_ListFile(const char *current_directory, void *pSession, FTPDCallBackReportFunct *pFunc, int socket)
{
    F_FIND find;
    char s[256];
    int rc;

    f_chdir("/");

    if (*current_directory)
    {
        if (f_chdir((char *)current_directory)) { return (FTPD_FAIL); }
    }

    rc = f_findfirst("*.*", &find);

    if (rc == 0)
    {
        // iprintf("\r\nDisplaying time/date information\r\n");
        do
        {
            if (!(find.attr & F_ATTR_DIR))
            {
                getdirstring(&find, s);
                gettimedate(&find);
                pFunc(socket, s);
            }
        } while (!f_findnext(&find));
        // iprintf("\r\n");
    }
    // else
    //{
    //   sniprintf( s, 256, "Card error: %d", rc );
    //   pFunc( socket, s );
    //}

    return (FTPD_OK);
}

/**
 * @brief Rename a file.
 */
int FTPD_Rename(const char *full_directory, const char *old_file_name, const char *new_file_name, void *pSession)
{
    f_chdir("/");

    if (*full_directory)
    {
        if (f_chdir((char *)full_directory)) { return (FTPD_FAIL); }
    }

    if (f_rename(old_file_name, new_file_name)) { return FTPD_FAIL; }

    return FTPD_OK;
}
