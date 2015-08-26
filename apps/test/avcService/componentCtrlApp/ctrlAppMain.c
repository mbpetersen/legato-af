//--------------------------------------------------------------------------------------------------
/**
 * @file userAppMain.c
 *
 * This component is used for testing the AirVantage Controller API.  It simulates a control
 * app that would register for update status notifications, and make decisions based on these
 * notifications.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
/**
 * Test case to execute
 */
//--------------------------------------------------------------------------------------------------
static int TestCase=0;


//--------------------------------------------------------------------------------------------------
/**
 * Test case 1: Download and Install any pending updates
 */
//--------------------------------------------------------------------------------------------------
static void DownloadAndInstall
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t progress
)
{
    le_avc_UpdateType_t updateType;

    switch ( updateStatus )
    {
        case LE_AVC_NO_UPDATE:
            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_AcceptDownload() == LE_FAULT );

            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_GetUpdateType(&updateType) == LE_FAULT );
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            LE_INFO("Total number of bytes to download = %d", totalNumBytes);
            LE_INFO("Download progress = %d%%", progress);

            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_AcceptInstall() == LE_FAULT );

            LE_WARN("Accept download");
            LE_ASSERT( le_avc_AcceptDownload() == LE_OK );

            // Verify update type
            LE_ASSERT( le_avc_GetUpdateType(&updateType) == LE_OK );
            //LE_ASSERT( updateType == LE_AVC_FIRMWARE_UPDATE );

            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_DeferDownload(3) == LE_FAULT );
            break;


        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_INFO("Download in Progress");
            LE_INFO("Total number of bytes to download = %d", totalNumBytes);
            LE_INFO("Download progress = %d%%", progress);
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_INFO("Download completed");
            LE_INFO("Total number of bytes to download = %d", totalNumBytes);
            LE_INFO("Download progress = %d%%", progress);
            break;

        case LE_AVC_INSTALL_PENDING:
            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_AcceptDownload() == LE_FAULT );

            LE_WARN("Accept install");
            LE_ASSERT( le_avc_AcceptInstall() == LE_OK );

            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_DeferInstall(3) == LE_FAULT );
            break;

        default:
            LE_WARN("Update status %i not handled", updateStatus);
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Test case 2: Continually defer the download for a minute at a time
 */
//--------------------------------------------------------------------------------------------------
static void DeferDownload
(
    le_avc_Status_t updateStatus
)
{
    switch ( updateStatus )
    {
        case LE_AVC_DOWNLOAD_PENDING:
            LE_WARN("Defer download");
            LE_ASSERT( le_avc_DeferDownload(1) == LE_OK );

            // In the wrong state, so this should be an error
            LE_ASSERT( le_avc_AcceptDownload() == LE_FAULT );
            break;

        default:
            LE_WARN("Update status %i not handled", updateStatus);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Test case 3: Initially defer the download for a few minutes, and then download and install.
 */
//--------------------------------------------------------------------------------------------------
static void DeferThenDownloadAndInstall
(
    le_avc_Status_t updateStatus
)
{
    // Use a count instead of a bool flag, so we could defer multiple times, if we want
    static int count=0;

    switch ( updateStatus )
    {
        case LE_AVC_DOWNLOAD_PENDING:
            if ( count < 1 )
            {
                LE_WARN("Defer download");
                LE_ASSERT( le_avc_DeferDownload(1) == LE_OK );

                // In the wrong state, so this should be an error
                LE_ASSERT( le_avc_AcceptDownload() == LE_FAULT );

                count++;
            }
            else
            {
                LE_WARN("Accept download");
                LE_ASSERT( le_avc_AcceptDownload() == LE_OK );
            }
            break;

        case LE_AVC_INSTALL_PENDING:
            LE_WARN("Accept install");
            LE_ASSERT( le_avc_AcceptInstall() == LE_OK );
            break;

        default:
            LE_WARN("Update status %i not handled", updateStatus);
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Test case 4: Download and Install any pending updates (simple version)
 * 
 * This is similar to test case 1, but without the extra testing and verification
 */
//--------------------------------------------------------------------------------------------------
static void SimpleDownloadAndInstall
(
    le_avc_Status_t updateStatus
)
{
    le_avc_UpdateType_t updateType;

    switch ( updateStatus )
    {
        case LE_AVC_NO_UPDATE:
            LE_WARN("No action");
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            if ( le_avc_GetUpdateType(&updateType) == LE_OK )
            {
                LE_INFO("Update type is %i", updateType);
            }
            else
            {
                LE_INFO("Update type is not available");
            }

            LE_WARN("Accept download");
            LE_ASSERT( le_avc_AcceptDownload() == LE_OK );
            break;

        case LE_AVC_INSTALL_PENDING:
            LE_WARN("Accept install");
            LE_ASSERT( le_avc_AcceptInstall() == LE_OK );
            break;

        default:
            LE_WARN("Update status %i not handled", updateStatus);
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Test case 5: Download, then defer install for a few minutes before installing
 *
 * Similar to test case 4, but with a defer before installing
 */
//--------------------------------------------------------------------------------------------------
static void SimpleDownloadAndDeferInstall
(
    le_avc_Status_t updateStatus
)
{
    // Use a count instead of a bool flag, so we could defer multiple times, if we want
    static int count=0;

    le_avc_UpdateType_t updateType;

    switch ( updateStatus )
    {
        case LE_AVC_NO_UPDATE:
            LE_WARN("No action");
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            if ( le_avc_GetUpdateType(&updateType) == LE_OK )
            {
                LE_INFO("Update type is %i", updateType);
            }
            else
            {
                LE_INFO("Update type is not available");
            }

            LE_WARN("Accept download");
            LE_ASSERT( le_avc_AcceptDownload() == LE_OK );
            break;

        case LE_AVC_INSTALL_PENDING:
            if ( count < 1 )
            {
                LE_WARN("Defer install");
                LE_ASSERT( le_avc_DeferInstall(1) == LE_OK );

                count++;
            }
            else
            {
                LE_WARN("Accept install");
                LE_ASSERT( le_avc_AcceptInstall() == LE_OK );
            }
            break;

        default:
            LE_WARN("Update status %i not handled", updateStatus);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Status handler
 *
 * Everything is driven from this handler
 */
//--------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t downloadProgress,
    void* contextPtr
)
{
    LE_ERROR("Got status %i", updateStatus);

    switch ( TestCase )
    {
        case 1:
            DownloadAndInstall(updateStatus, totalNumBytes, downloadProgress);
            break;

        case 2:
            DeferDownload(updateStatus);
            break;

        case 3:
            DeferThenDownloadAndInstall(updateStatus);
            break;

        case 4:
            SimpleDownloadAndInstall(updateStatus);
            break;

        case 5:
            SimpleDownloadAndDeferInstall(updateStatus);
            break;

        default:
            LE_ERROR("Invalid test case %i", TestCase);
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Init the component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{

    if (le_arg_NumArgs() >= 1)
    {
        const char* arg1 = le_arg_GetArg(0);
        int testCase = atoi(arg1);

        if ( testCase > 0 )
        {
            TestCase = testCase;
        }
    }

    // The default test case of 0 just starts a session, and nothing else.  The remaining test
    // cases will respond to update notifications, and so the details of these test cases are
    // in the registered StatusHandler() function.

    if ( TestCase > 0 )
    {
        le_avc_AddStatusEventHandler(StatusHandler, NULL);
    }

    le_result_t result = le_avc_StartSession();
    LE_INFO("After calling le_avc_StartSession(), result=%i", result);
}

