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
 * @file   main.cpp
 * @brief  WebGL Example
 *
 * In this example, we look at how to take advantage some of the more recent web technologies available
 * by building a page that utilizes WebGL and a really nice wrapper library, Three.js, to render
 * a simple textured model.  We then update the position and rotation of that model using a WebSocket
 * connection. The main points of this example will show how to incorporate an SD card in order to hold
 * all of the model and texture data, as well as the JavaScript libraries, how to use the NetBurner as
 * an FTP server to be able to remotely update your assets, and how to get your scene up and running.
 */

// NB Constants
#include <constants.h>

// NB Libs
#include <buffers.h>
#include <http.h>
#include <init.h>
#include <iosys.h>
#include <stdlib.h>
#include <webclient/json_lexer.h>

// NB FTP
#include <ftpd.h>

#if (defined(USE_MMC) && defined(MOD5441X))
#define MULTI_MMC TRUE   // For modules with onboard flash sockets, even if you are using external flash cards
#include <effs_fat/multi_drive_mmc_mcf.h>
#elif (defined(USE_MMC))
#include <effs_fat/mmc_mcf.h>
#elif (defined(USE_CFC))
#include <effs_fat/cfc_mcf.h>
#endif

#include "FileSystemUtils.h"
#include "cardtype.h"
#include "web.h"

// The FTP task priority
#define FTP_PRIO (MAIN_PRIO - 2)

const char *AppName = "WebGL Example";

// Our WebSocket file descriptor
extern int ws_fd;

// These are buffers that we use to send JSON data to clients
const int ReportBufSize = 512;
char ReportBuffer[ReportBufSize];

// These are the global values that we will use managing the position and rotation
// of the model.
float Pos[3] = {0.0, 0.0, 0.0};
float GoalPos[3] = {0.0, 0.0, 0.0};
float Rot[3] = {0.0, 0.0, 0.0};       // Rotations are stored/sent as radians
float GoalRot[3] = {0.0, 0.0, 0.0};   // Rotations are stored/sent as radians

extern "C"
{
    void UserMain(void *pd);
}

/**
 * @brief Determines how much to change a position or rotation value for one
 * step.
 */
inline float GetMovVal(float dif)
{
    float difSign = dif < 0.0 ? -1 : 1;
    return ((dif * difSign) >= 0.025) ? 0.025 * difSign : dif;
}

/**
 * @brief This function manages the goal positions and rotations for our model to move to.
 * Once we get within a certain distance of the goal value, we pick new ones.
 *
 * When using an actual sensor, this is the function that should get updated to use real values.
 * The rotation values are expected to be in radians.
 */
void UpdatePosAndRot()
{
    // Pick new goal position
    // Find the distance between the two
    float xDif = GoalPos[0] - Pos[0];
    float yDif = GoalPos[1] - Pos[1];
    float zDif = GoalPos[2] - Pos[2];
    float dist = sqrt((xDif * xDif) + (yDif * yDif) + (zDif * zDif));

    // Close enough, pick new points
    if (dist < 0.01)
    {
        for (int i = 0; i < 3; i++)
        {
            float posSign = ((rand() % 2) == 0) ? -1 : 1;
            GoalPos[i] = (rand() % 2) * posSign;   // The 2 here is an arbitrary value to keep the model on the screen
        }
    }
    else
    {
        Pos[0] += GetMovVal(xDif);
        Pos[1] += GetMovVal(yDif);
        Pos[2] += GetMovVal(zDif);
    }

    // Pick new goal rotation
    // Find the distance between the two
    xDif = GoalRot[0] - Rot[0];
    yDif = GoalRot[1] - Rot[1];
    zDif = GoalRot[2] - Rot[2];
    dist = sqrt((xDif * xDif) + (yDif * yDif) + (zDif * zDif));

    // Close enough, pick new rotation values
    if (dist < 0.01)
    {
        for (int i = 0; i < 3; i++)
        {
            float rotSign = ((rand() % 2) == 0) ? -1 : 1;
            GoalRot[i] = ((rand() % 7855) / 10000.0) * rotSign;   // .7855 is a hair over 45 degrees
        }
    }
    else
    {
        Rot[0] += GetMovVal(xDif);
        Rot[1] += GetMovVal(yDif);
        Rot[2] += GetMovVal(zDif);
    }
}

/**
 * @brief This function packages up our rotation and position data, and sends it out the
 * file descriptor used by the WebSocket connection.
 */
void SendWebSocketData()
{
    // Our JSON blob that we will send
    ParsedJsonDataSet jsonOutObj;

    // Build the JSON blob
    jsonOutObj.StartBuilding();
    jsonOutObj.AddObjectStart("PosUpdate");
    jsonOutObj.Add("x", Pos[0]);
    jsonOutObj.Add("y", Pos[1]);
    jsonOutObj.Add("z", Pos[2]);
    jsonOutObj.EndObject();
    jsonOutObj.AddObjectStart("RotUpdate");
    jsonOutObj.Add("x", Rot[0]);
    jsonOutObj.Add("y", Rot[1]);
    jsonOutObj.Add("z", Rot[2]);
    jsonOutObj.EndObject();
    jsonOutObj.DoneBuilding();

    // If you would like to print the JSON object to serial to see the format, uncomment the next line
    // JsonOutObject.PrintObject(true);

    // Print JSON object to a buffer and write the buffer to the WebSocket file descriptor
    int dataLen = jsonOutObj.PrintObjectToBuffer(ReportBuffer, ReportBufSize);
    writeall(ws_fd, ReportBuffer, dataLen);
}

/**
 * @brief This is where our main application begins.
 */
void UserMain(void *pd)
{
    init();
    OSChangePrio(MAIN_PRIO);

    /**
     * The following call to f_enterFS() must be called in every task that accesses
     * the file system.  This must only be called once in each task and must be done before
     * any other file system functions are used.  Up to 10 tasks can be assigned to use
     * the file system. Any task may also be removed from accessing the file system with a
     * call to the function f_releaseFS().
     */
    f_enterFS();

    // We must also enter the file system for the HTTP tasks
    OSChangePrio(HTTP_PRIO);
    f_enterFS();

    // We must also enter the file system for the FTP tasks
    OSChangePrio(FTP_PRIO);
    f_enterFS();

    OSChangePrio(MAIN_PRIO);

    // Initialize the CFC or SD/MMC external flash drive
    InitExtFlash();

    // Initialize the stack, set up the web server, etc.
    StartHTTP();

    RegisterWebFuncs();

    // Start FTP server with a task priority higher than UserMain()
    int status = FTPDStart(21, FTP_PRIO);
    if (status == FTPD_OK)
    {
        iprintf("Started FTP Server\r\n");
        if (F_LONGFILENAME == 1) { iprintf("Long file names are supported\r\n"); }
        else
        {
            iprintf("Long file names are not supported- only 8.3 format\r\n");
        }
    }
    else
    {
        iprintf("** Error: %d. Could not start FTP Server\r\n", status);
    }

    iprintf("Starting WebGL Example\r\n");

    DumpDir();
    while (1)
    {
        // Update the simulations position and rotation values
        UpdatePosAndRot();

        // If we have a valid WebSocket file descriptor
        if (ws_fd > 0)
        {
            // Send our data
            SendWebSocketData();
        }
        OSTimeDly(1);
    }
}
