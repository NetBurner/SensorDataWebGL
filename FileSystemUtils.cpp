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
 * EFFS FAT32 file system utilities for use with SD and microSD flash cards.
 *
 * Modules with an onboard microSD flash socket should use the multi MMC header
 * files and functions because the modules are capable of supporting both onboard
 * and external flash cards (even if you application only uses one).
 */

// NB Libs
#include <ctype.h>
#include <stdio.h>
#include <utils.h>

// EFFS Files
#include <effs_fat/fat.h>
#include <effs_fat/effs_utils.h>

#include "cardtype.h"
#include "FileSystemUtils.h"

#if (defined(USE_MMC) && defined(MOD5441X))
#define MULTI_MMC TRUE
#define EXTERNAL_DRIVE_NUM 0
#define ONBOARD_DRIVE_NUM 1
int flashDriveNum = EXTERNAL_DRIVE_NUM;
#include <effs_fat/multi_drive_mmc_mcf.h>   // For modules with onboard flash sockets, even if you are using external flash cards
#elif (defined(USE_MMC))
#include <effs_fat/mmc_mcf.h>
#elif (defined(USE_CFC))
#include <effs_fat/cfc_mcf.h>
#endif

char driveType[20];

// Error codes that can be encountered while using EFFS
char EffsErrorCode[][80] = {
    "F_NO_ERROR",                // 0
    "F_ERR_INVALIDDRIVE",        // 1
    "F_ERR_NOTFORMATTED",        // 2
    "F_ERR_INVALIDDIR",          // 3
    "F_ERR_INVALIDNAME",         // 4
    "F_ERR_NOTFOUND",            // 5
    "F_ERR_DUPLICATED",          // 6
    "F_ERR_NOMOREENTRY",         // 7
    "F_ERR_NOTOPEN",             // 8
    "F_ERR_EOF",                 // 9
    "F_ERR_RESERVED",            // 10
    "F_ERR_NOTUSEABLE",          // 11
    "F_ERR_LOCKED",              // 12
    "F_ERR_ACCESSDENIED",        // 13
    "F_ERR_NOTEMPTY",            // 14
    "F_ERR_INITFUNC",            // 15
    "F_ERR_CARDREMOVED",         // 16
    "F_ERR_ONDRIVE",             // 17
    "F_ERR_INVALIDSECTOR",       // 18
    "F_ERR_READ",                // 19
    "F_ERR_WRITE",               // 20
    "F_ERR_INVALIDMEDIA",        // 21
    "F_ERR_BUSY",                // 22
    "F_ERR_WRITEPROTECT",        // 23
    "F_ERR_INVFATTYPE",          // 24
    "F_ERR_MEDIATOOSMALL",       // 25
    "F_ERR_MEDIATOOLARGE",       // 26
    "F_ERR_NOTSUPPSECTORSIZE",   // 27
    "F_ERR_DELFUNC",             // 28
    "F_ERR_MOUNTED",             // 29
    "F_ERR_TOOLONGNAME",         // 30
    "F_ERR_NOTFORREAD",          // 31
    "F_ERR_DELFUNC",             // 32
    "F_ERR_ALLOCATION",          // 33
    "F_ERR_INVALIDPOS",          // 34
    "F_ERR_NOMORETASK",          // 35
    "F_ERR_NOTAVAILABLE",        // 36
    "F_ERR_TASKNOTFOUND",        // 37
    "F_ERR_UNUSABLE"             // 38
};

void f_open_PrintError(char *pFileName);
void f_close_PrintError(char *pFileName);

/**
 * @brief Displays any EFFS error codes to stdio
 */
void DisplayEffsErrorCode(int code)
{
    if (code <= MAX_EFFS_ERRORCODE) { iprintf("%s\r\n", EffsErrorCode[code]); }
    else
    {
        iprintf("Unknown EFFS error code [%d]\r\n", code);
    }
}

/**
 * @brief Initialization function for setting up EFFS on
 * MultiMedia/Secure Digital cards or Comact Flash cards.
 */
BYTE InitExtFlash()
{
    // Card detection check
#if (defined MULTI_MMC)
    while (get_cd(flashDriveNum) == 0)
    {
        iprintf("No MMC/SD card detected on drive %d. Insert a card and then press <Enter>\r\n", flashDriveNum);
        getchar();
    }
#elif (defined USE_MMC)
    while (get_cd() == 0)
    {
        iprintf("No MMC/SD card detected. Insert a card and then press <Enter>\r\n");
        getchar();
    }
#endif

    // Write protection check
#if (defined MULTI_MMC)
    while (get_wp(flashDriveNum) == 1)
    {
        iprintf("SD/MMC Card is write-protected. Disable write protection then press <Enter>\r\n");
        getchar();
    }
#elif (defined USE_MMC)
    while (get_wp() == 1)
    {
        iprintf("SD/MMC Card is write-protected. Disable write protection then press <Enter>\r\n");
        getchar();
    }
#endif

    int rv;

    /**
     * The f_mountfat() function is called to mount a FAT drive. Although
     * there are parameters in this function, they should not be modified.
     * The function call should be used as it appears for a Compact Flash
     * card. For reference, the parameters are:
     *
     * drive_num:   Set to MMC_DRV_NUM, the drive to be mounted
     * p_init_func: Points to the initialization function for the drive.
     *              For the Compact Flash drive, the function is located
     *              in \nburn\<platform>\system\cfc_mem_nburn.c.
     * p_user_info: Used to pass optional information. In this case, the
     *              drive number.
     *
     * The return values are:
     *    F_NO_ERROR:  drive successfully mounted
     *    Any other value: Error occurred. See page 22 in the HCC-Embedded
     *    file system manual for the list of error codes.
     */
    sniprintf(driveType, 20, "No Drive");
#if (defined MULTI_MMC)
    iprintf("Mounting drive %d in MULTI_MMC mode\r\n", flashDriveNum);
    sniprintf(driveType, 20, "SD/MMC");
    rv = f_mountfat(MMC_DRV_NUM, mmc_initfunc, flashDriveNum);
#elif (defined USE_MMC)
    iprintf("Mounting drive USE_MMC mode\r\n");
    sniprintf(driveType, 20, "SD/MMC");
    rv = f_mountfat(MMC_DRV_NUM, mmc_initfunc, F_MMC_DRIVE0);
#elif (defined USE_CFC)
    iprintf("Mounting drive USE_CFC mode\r\n");
    sniprintf(driveType, 20, "CFC");
    rv = f_mountfat(CFC_DRV_NUM, cfc_initfunc, F_CFC_DRIVE0);
#endif
    if (rv == F_NO_ERROR)
    {
        iprintf("FAT mount to %s successful\r\n", driveType);
    }
    else
    {
        iprintf("FAT mount to %s failed: ", driveType);
        DisplayEffsErrorCode(rv);
        return rv;
    }

    /**
     * Change to SD/MMC drive
     * We need to call the change function to access the new drive. Note
     * that ANY function other than the f_mountfat() is specific to a task.
     * This means that if f_chdrive() is called in a different task to a
     * different drive, it will not affect this task - this task will
     * remain on the same drive.
     */
    rv = f_chdrive(EXT_FLASH_DRV_NUM);

    if (rv == F_NO_ERROR)
    {
        iprintf("%s drive change successful\r\n", driveType);
    }
    else
    {
        iprintf("%s drive change failed: ", driveType);
        DisplayEffsErrorCode(rv);
    }

    return rv;
}

/**
 * @brief Un-mounts the SD card.
 */
BYTE UnmountExtFlash()
{
    int rv;
    iprintf("Unmounting %s card\r\n\r\n", driveType);
    rv = f_delvolume(EXT_FLASH_DRV_NUM);
    if (rv != F_NO_ERROR)
    {
        iprintf("*** Error in f_delvolume(): ");
        DisplayEffsErrorCode(rv);
    }
    return rv;
}

/**
 * @brief Formats the SD card.
 */
BYTE FormatExtFlash(long FATtype)
{
    int rv;
    iprintf("Formatting %s card\r\n\r\n", driveType);
    rv = f_format(EXT_FLASH_DRV_NUM, FATtype);
    if (rv != F_NO_ERROR)
    {
        iprintf("*** Error in f_format(): ");
        DisplayEffsErrorCode(rv);
    }
    return rv;
}

/**
 * @brief Display space used, total and bad.
 */
BYTE DisplayEffsSpaceStats()
{
    F_SPACE space;
    volatile int rv;
    iprintf("Retrieving external flash usage...\r\n");
    rv = f_getfreespace(EXT_FLASH_DRV_NUM, &space);

    if (rv == F_NO_ERROR)
    {
        iprintf("Flash card memory usage (bytes):\r\n");
        long long totalSize = space.total_high;
        totalSize = ((totalSize << 32) + space.total);

        long long freeSize = space.free_high;
        freeSize = ((freeSize << 32) + space.free);

        long long usedSize = space.used_high;
        usedSize = ((usedSize << 32) + space.used);

        long long badSize = space.bad_high;
        badSize = ((badSize << 32) + space.bad);

        iprintf("%llu total, %llu free, %llu used, %llu bad\r\n", totalSize, freeSize, usedSize, badSize);
        // iprintf( "%lu total, %lu free, %lu used, %lu bad\r\n", space.total, space.free, space.used, space.bad );
    }
    else
    {
        iprintf("\r\n*** Error in f_getfreepace(): ");
        DisplayEffsErrorCode(rv);
    }

    return rv;
}

/**
 * @brief Displays a list of all directories and files in the
 * file system.
 */
BYTE DumpDir()
{
    F_FIND finder;   // location to store the information retrieved

    /**
     * Find first file or subdirectory in specified directory. First call the
     * f_findfirst function, and if file was found get the next file with
     * f_findnext function. Files with the system attribute set will be ignored.
     * Note: If f_findfirst() is called with "*.*" and the current directory is
     * not the root directory, the first entry found will be "." - the current
     * directory.
     */
    volatile int rc = f_findfirst("*.*", &finder);
    if (rc == F_NO_ERROR)   // found a file or directory
    {
        do
        {
            if ((finder.attr & F_ATTR_DIR))
            {
                iprintf("Found Directory [%s]\r\n", finder.filename);

                if (finder.filename[0] != '.')
                {
                    f_chdir(finder.filename);
                    DumpDir();
                    f_chdir("..");
                }
            }
            else
            {
                iprintf("Found File [%s] : %lu Bytes\r\n", finder.filename, finder.filesize);
            }
        } while (!f_findnext(&finder));
    }

    return rc;
}

/**
 * @brief Deletes the file and prints the status. Returns 0 for success.
 */
BYTE DeleteFile(char *pFileName)
{
    volatile int rv;
    rv = f_delete(pFileName);
    if (rv != F_NO_ERROR)
    {
        if (OSTaskName() != nullptr)
        {
            iprintf("\r\n*** Error in f_delete( %s ) during task(%s)\r\n", pFileName, OSTaskName());
        }
        else
        {
            iprintf("\r\n*** Error in f_delete( %s ) during task(%d)\r\n", pFileName, OSTaskID());
        }
        DisplayEffsErrorCode(rv);
    }
    return rv;
}

/**
 * @brief Opens a file and writes data from a buffer to that file. It then closes the file and then prints the status.
 * Returns 0 for success.
 */
DWORD WriteFile(BYTE *pDataToWrite, char *pFileName, DWORD NumBytes)
{
    DWORD rvW, rvC;
    F_FILE *fp = f_open(pFileName, "w+");
    if (fp)
    {
        rvW = f_write(pDataToWrite, 1, NumBytes, fp);
        if (rvW != NumBytes) iprintf("\r\n*** Error in f_write(%s): %lu out of %lu bytes writte\r\n", pFileName, rvW, NumBytes);

        rvC = f_close(fp);   // Close a previously opened file of type F_FILE
        if (rvC != F_NO_ERROR)
        {
            f_close_PrintError(pFileName);
            DisplayEffsErrorCode(rvC);
            return 0;
        }
    }
    else
    {
        f_open_PrintError(pFileName);
        return 0;
    }

    return rvW;
}

/**
 *  @brief Opens file then writes to it starting at the end of data in file.  Closes the file and prints status.
 *  Returns 0 for success.
 */
DWORD AppendFile(BYTE *pDataToWrite, char *pFileName, DWORD NumBytes)
{
    DWORD rvA, rvC;
    F_FILE *fp = f_open(pFileName, "a+");
    if (fp)
    {
        rvA = f_write(pDataToWrite, 1, NumBytes, fp);
        if (rvA != NumBytes) iprintf("\r\n*** Error in f_write(%s): %lu out of %lu bytes written\r\n", pFileName, rvA, NumBytes);

        rvC = f_close(fp);   // Close a previously opened file of type F_FILE
        if (rvC != F_NO_ERROR)
        {
            f_close_PrintError(pFileName);
            DisplayEffsErrorCode(rvC);
            return 0;
        }
    }
    else
    {
        f_open_PrintError(pFileName);
        return 0;
    }

    return rvA;
}

/**
 * @brief Opens a file, reads its contents into a buffer, then closes the file, and prints the status.
 * Returns 0 for success
 */
DWORD ReadFile(BYTE *pReadBuffer, char *pFileName, DWORD NumBytes)
{
    DWORD rvR, rvC;
    F_FILE *fp = f_open(pFileName, "r");
    if (fp)
    {
        rvR = (DWORD)f_read(pReadBuffer, 1, NumBytes, fp);
        // if( NumBytes != rvR )
        //{
        //    iprintf( "*** Warning in ReadFile(%s): %lu out of %lu bytes read\r\n", pFileName, rvR, NumBytes );
        //}

        rvC = f_close(fp);   // Close a previously opened file of type F_FILE
        if (rvC != F_NO_ERROR)
        {
            f_close_PrintError(pFileName);
            DisplayEffsErrorCode(rvC);
            return 0;
        }
    }
    else
    {
        f_open_PrintError(pFileName);
        return 0;
    }
    return rvR;
}

/**
 * @brief This function will read and write files/data to demonstrate basic file system operation.
 */
void ReadWriteTest(char *FileName)
{
    /**
     * Create a test file
     * The f_open() function opens a file for reading or writing. The following
     * modes are allowed to open:
     *    "r"   Open existing file for reading. The stream is positioned at the
     *          beginning of the file.
     *    "r+"  Open existing file for reading and writing. The stream is
     *          positioned at the beginning of the file.
     *    "w"   Truncate file to zero length or create file for writing. The
     *          stream is positioned at the beginning of the file.
     *    "w+"  Open a file for reading and writing. The file is created if it
     *          does not exist, otherwise it is truncated. The stream is
     *          positioned at the beginning of the file.
     *    "a"   Open for appending (writing to end of file). The file is created
     *          if it does not exist. The stream is positioned at the end of the
     *          file.
     *    "a+"  Open for reading and appending (writing to end of file). The file
     *          is created if it does not exist. The stream is positioned at the
     *          end of the file.
     * Note: There is no text mode. The system assumes all files to be accessed in
     * binary mode only.
     */
    iprintf("\r\nCreating test file: %s\r\n", FileName);
    F_FILE *fp = f_open(FileName, "w+");
    if (fp)
    {
        for (int i = 0; i < 5; i++)
        {
            #define WRITE_BUFSIZE 80
            char write_buf[WRITE_BUFSIZE];
            sniprintf(write_buf, WRITE_BUFSIZE, "Hello World %d\r\n", i);
            /**
             * f_write( const void *buffer,  // pointer to data to be written
             * long size,           // size of items to be written
             * long size size_st,   // number of items to be written
             * F_FILE )             // handle of target file
             *
             * Returns the number of items written.
             */
            int n = f_write(write_buf, 1, strlen(write_buf), fp);
            iprintf("Wrote %d bytes: %s", n, write_buf);
        }

        // Read the data in the test file
        iprintf("\r\nRewinding file\r\n");
        int rv = f_rewind(fp);   // set current file pointer to start of file
        if (rv != F_NO_ERROR)
        {
            f_close_PrintError(FileName);
            DisplayEffsErrorCode(rv);
        }
        else
        {
            while (!f_eof(fp))
            {
                /**
                 * Read bytes from the current position in the target file. File has
                 * to be opened with “r”, "r+", "w+" or "a+".
                 * f_read ( const void *buffer,  // pointer to buffer to store data
                 *          long size,           // size of items to be read
                 *          long size size_st,   // number of items to be read
                 *          F_FILE )             // handle of target file
                 * Returns the number of items read.
                 */
                #define READ_BUFSIZE 80
                char read_buf[READ_BUFSIZE];
                int n = f_read(read_buf, 1, READ_BUFSIZE - 1, fp);
                read_buf[n] = '\0';   // terminate string
                iprintf("Read %d bytes:\r\n%s\r\n", n, read_buf);
            }

            iprintf("Closing file %s\r\n\r\n", FileName);
            rv = f_close(fp);   // Close a previously opened file of type F_FILE
            if (rv != F_NO_ERROR)
            {
                f_close_PrintError(FileName);
                DisplayEffsErrorCode(rv);
            }
        }
    }
    else
    {
        f_open_PrintError(FileName);
    }
}

/**
 * @brief Tests f_fgets().
 */
void fgets_test(char *FileName)
{
    iprintf("\r\nOpening test file for reading: %s\r\n", FileName);
    F_FILE *fp = f_open(FileName, "r");
    if (fp)
    {
        iprintf("Calling fgets() until end of file\r\n");
        char buf[128];
        while (!f_eof(fp))
        {
            char *buf_rtn = f_fgets(buf, 128, fp);
            if (buf_rtn != nullptr)
            {
                iprintf("fgets() returned: \"");
                for (int i = 0; i < (int)strlen(buf); i++)
                {
                    if (isprint(buf[i]))
                        iprintf("%c", buf[i]);
                    else
                        iprintf("<0x%X>", buf[i]);
                }
                iprintf("\"\r\n");
            }
            else
                iprintf("End of file\r\n");
        }
    }
    else
    {
        f_open_PrintError(FileName);
    }

    iprintf("Closing file %s\r\n\r\n", FileName);
    int rv = f_close(fp);   // Close a previously opened file of type F_FILE
    if (rv != F_NO_ERROR)
    {
        f_close_PrintError(FileName);
        DisplayEffsErrorCode(rv);
    }
}

/**
 * @brief Displays a text file.
 */
void DisplayTextFile(char *FileName)
{
    iprintf("\r\nOpening test file for reading: %s\r\n", FileName);
    F_FILE *fp = f_open(FileName, "r");
    if (fp)
    {
        while (!f_eof(fp))
        {
            /**
             * Read bytes from the current position in the target file. File has
             * to be opened with “r”, "r+", "w+" or "a+".
             * f_read ( const void *buffer,  // pointer to buffer to store data
             *          long size,           // size of items to be read
             *          long size size_st,   // number of items to be read
             *          F_FILE )             // handle of target file
             * Returns the number of items read.
             */
            #define DISP_READ_BUFSIZE 255
            char read_buf[DISP_READ_BUFSIZE];
            int n = f_read(read_buf, 1, DISP_READ_BUFSIZE - 1, fp);
            read_buf[n] = '\0';   // terminate string
            iprintf("Read %d bytes:\r\n%s\r\n", n, read_buf);
        }

        iprintf("Closing file %s\r\n\r\n", FileName);
        int rv = f_close(fp);   // Close a previously opened file of type F_FILE
        if (rv != F_NO_ERROR)
        {
            f_close_PrintError(FileName);
            DisplayEffsErrorCode(rv);
        }
    }
    else
    {
        f_open_PrintError(FileName);
    }
}

/**
 * @brief Tests f_fprintf().
 */
void fprintf_test()
{
    /**
     * Open the test file
     * The f_open() function opens a file for reading or writing. The following
     * modes are allowed to open:
     *    "r"   Open existing file for reading. The stream is positioned at the
     *          beginning of the file.
     *    "r+"  Open existing file for reading and writing. The stream is
     *          positioned at the beginning of the file.
     *    "w"   Truncate file to zero length or create file for writing. The
     *          stream is positioned at the beginning of the file.
     *    "w+"  Open a file for reading and writing. The file is created if it
     *          does not exist, otherwise it is truncated. The stream is
     *          positioned at the beginning of the file.
     *    "a"   Open for appending (writing to end of file). The file is created
     *          if it does not exist. The stream is positioned at the end of the
     *          file.
     *    "a+"  Open for reading and appending (writing to end of file). The file
     *          is created if it does not exist. The stream is positioned at the
     *          end of the file.
     * Note: There is no text mode. The system assumes all files to be accessed in
     * binary mode only.
     */

    char *FileName = "TestFile.txt";   // 8.3 file names supported by default
    static WORD WriteCount;

    iprintf("\r\nOpening test file for appending: %s\r\n", FileName);
    F_FILE *fp = f_open(FileName, "a");
    if (fp)
    {
        f_fprintf(fp, "Write #%u, Secs = %lu, Secs = 0x%lX\r\n", WriteCount, Secs, Secs);
        int rv = f_close(fp);
        if (rv != F_NO_ERROR)
        {
            f_close_PrintError(FileName);
            DisplayEffsErrorCode(rv);
        }

        iprintf("Wrote to file: \"Write #%u, Secs = %lu, Secs = 0x%lX\"\r\n", WriteCount, Secs, Secs);
        WriteCount++;
    }
    else
    {
        f_open_PrintError(FileName);
    }

    DisplayTextFile(FileName);
}

/**
 * @brief Tests f_puts().
 */
void fputs_test(char *FileName)
{
    /**
     * Open the test file
     * The f_open() function opens a file for reading or writing. The following
     * modes are allowed to open:
     *    "r"   Open existing file for reading. The stream is positioned at the
     *          beginning of the file.
     *    "r+"  Open existing file for reading and writing. The stream is
     *          positioned at the beginning of the file.
     *    "w"   Truncate file to zero length or create file for writing. The
     *          stream is positioned at the beginning of the file.
     *    "w+"  Open a file for reading and writing. The file is created if it
     *          does not exist, otherwise it is truncated. The stream is
     *          positioned at the beginning of the file.
     *    "a"   Open for appending (writing to end of file). The file is created
     *          if it does not exist. The stream is positioned at the end of the
     *          file.
     *    "a+"  Open for reading and appending (writing to end of file). The file
     *          is created if it does not exist. The stream is positioned at the
     *          end of the file.
     * Note: There is no text mode. The system assumes all files to be accessed in
     * binary mode only.
     */

    iprintf("\r\nOpening test file for appending: %s\r\n", FileName);
    F_FILE *fp = f_open(FileName, "a");
    if (fp)
    {
        char s[128];
        sniprintf(s, 128, "f_fputs() executed at %ld seconds\r\n", Secs);
        int n = f_fputs(s, fp);

        int rv = f_close(fp);
        if (rv != F_NO_ERROR)
        {
            f_close_PrintError(FileName);
            DisplayEffsErrorCode(rv);
        }

        iprintf("Wrote %d bytes to file: \"%s\"\r\n", n, s);
    }
    else
    {
        f_open_PrintError(FileName);
    }

    DisplayTextFile(FileName);
}

/**
 * @brief Prints errors associated with f_open().
 */
void f_open_PrintError(char *pFileName)
{
    if (OSTaskName() != nullptr)
    {
        iprintf("*** Error in f_open(%s) during task(%s)\r\n", pFileName, OSTaskName());
    }
    else
    {
        iprintf("*** Error in f_open(%s) during task(%d)\r\n", pFileName, OSTaskID());
    }

    int rv = f_getlasterror();
    DisplayEffsErrorCode(rv);
}

/**
 * @brief Prints errors associated with f_close().
 */
void f_close_PrintError(char *pFileName)
{
    if (OSTaskName() != nullptr)
    {
        iprintf("*** Error in f_close(%s) during task(%s)\r\n", pFileName, OSTaskName());
    }
    else
    {
        iprintf("*** Error in f_close(%s) during task(%d)\r\n", pFileName, OSTaskID());
    }
}

