#include "legato.h"
#include "interfaces.h"
#include "le_mdc_local.h"
#include "log.h"
#include "pa_mdc.h"
#include "pa_mdc_simu.h"
#include "pa_mrc_simu.h"
#include "pa_sim_simu.h"
#include "args.h"

#define NB_PROFILE  5
#define IP_STR_SIZE     16

le_log_SessionRef_t LE_LOG_SESSION;
le_log_Level_t* LE_LOG_LEVEL_FILTER_PTR;
static le_sem_Ref_t    ThreadSemaphore;
static le_mdc_ProfileRef_t ProfileRef[NB_PROFILE];
static le_mdc_SessionStateHandlerRef_t SessionStateHandler[NB_PROFILE];
static le_mdc_ProfileRef_t ProfileRefReceivedByHandler = NULL;
static bool ConnectionStateReceivedByHandler = false;

//--------------------------------------------------------------------------------------------------
/**
 * The goal of this test is to:
 * - Allocate profile.
 * - Test the configuration of profiles.
 *
 * API tested:
 * - le_mdc_GetProfile
 * - le_mdc_GetProfileFromApn
 * - le_mdc_GetProfileIndex
 * - le_mdc_GetAuthentication / le_mdc_SetAuthentication
 * - le_mdc_GetPDP / le_mdc_SetPDP
 * - le_mdc_GetAPN / le_mdc_SetAPN
 * - le_mdc_SetDefaultAPN
 *
 * Exit if failed
 */
//--------------------------------------------------------------------------------------------------
static void TestMdc_Configuration( void )
{
    pa_mrcSimu_SetRadioAccessTechInUse(LE_MRC_RAT_GSM);

    pa_mdc_ProfileData_t profileData;
    char newAPN[] = "NewAPN";
    int i;
    le_result_t res;

    /* Configure the pa_mdcSimu */
    for (i = 0; i < NB_PROFILE; i++)
    {
        char tstAPN[10];
        snprintf(tstAPN,10,"TstAPN%d", i);
        memset(&profileData,0,sizeof(pa_mdc_ProfileData_t));
        strcpy( &profileData.apn[0], tstAPN);
        profileData.authentication.type = LE_MDC_AUTH_NONE;
        profileData.pdp = LE_MDC_PDP_IPV4;
        pa_mdcSimu_SetProfile(i+1, &profileData);
    }

    /* Allocate profiles: use le_mdc_GetProfileFromApn or le_mdc_GetProfile */
    for (i = 0; i < NB_PROFILE; i++)
    {
        char tstAPN[10];
        snprintf(tstAPN,10,"TstAPN%d", i);

        if ( (i/2)*2 == i )
        {
            res = le_mdc_GetProfileFromApn(tstAPN, &ProfileRef[i]);
            LE_ASSERT(res == LE_OK);
        }
        else
        {
            ProfileRef[i] = le_mdc_GetProfile(i+1);
        }

        /* expected value: ProfileRef not null */
        LE_ASSERT(ProfileRef[i] != NULL);

        /* Check the index*/
        LE_ASSERT((i+1) == le_mdc_GetProfileIndex(ProfileRef[i]));
    }

    /* Get and change APN of 1st profile */
    char apn[10];
    res = le_mdc_GetAPN(ProfileRef[0], apn, sizeof(apn));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(strcmp("TstAPN0", apn)==0);
    res = le_mdc_SetAPN(ProfileRef[0], newAPN);
    LE_ASSERT(res == LE_OK);
    res = le_mdc_GetAPN(ProfileRef[0], apn, sizeof(apn));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(strcmp(newAPN, apn)==0);

    /* Check to get a profile thanks to its APN */
    le_mdc_ProfileRef_t profile;
    res = le_mdc_GetProfileFromApn("TstAPN0", &profile);
    LE_ASSERT(res == LE_NOT_FOUND);
    res = le_mdc_GetProfileFromApn(newAPN, &profile);
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(profile == ProfileRef[0]);

    /* Get and change authentification */
    le_mdc_Auth_t auth;
    char userName[10];
    char password[10];
    char myUserName[]="myName";
    char myPassword[]="myPwd";
    res = le_mdc_GetAuthentication(ProfileRef[0],&auth, userName, sizeof(userName), password,
                                                                                sizeof(password));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(auth == LE_MDC_AUTH_NONE);
    res = le_mdc_SetAuthentication(ProfileRef[0],LE_MDC_AUTH_PAP, myUserName, myPassword);
    LE_ASSERT(res == LE_OK);
    res = le_mdc_GetAuthentication(ProfileRef[0],&auth, userName, sizeof(userName), password,
                                                                                sizeof(password));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(auth == LE_MDC_AUTH_PAP);
    LE_ASSERT(strcmp(userName, myUserName)==0);
    LE_ASSERT(strcmp(password, myPassword)==0);

    /* Get PDP type and change it */
    le_mdc_Pdp_t pdp;
    pdp = le_mdc_GetPDP(ProfileRef[0]);
    LE_ASSERT(pdp == LE_MDC_PDP_IPV4);
    res = le_mdc_SetPDP(ProfileRef[0], LE_MDC_PDP_IPV6);
    LE_ASSERT(res == LE_OK);
    pdp = le_mdc_GetPDP(ProfileRef[0]);
    LE_ASSERT(pdp == LE_MDC_PDP_IPV6);

    /* start a session: profile can't be modified when a session is activated */
    res = le_mdc_StartSession(ProfileRef[0]);
    LE_ASSERT(res == LE_OK);

    /* Try to set APN (error is expected as a connection is in progress) */
    res = le_mdc_SetAPN(ProfileRef[0], "TstAPN0");
    LE_ASSERT(res == LE_FAULT);
    /* Get is possible */
    res = le_mdc_GetAPN(ProfileRef[0], apn, sizeof(apn));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(strcmp(newAPN, apn)==0);

    /* Try to set authentification (error is expected as a connection is in progress) */
    res = le_mdc_SetAuthentication(ProfileRef[0],LE_MDC_AUTH_CHAP, myUserName, myPassword);
    LE_ASSERT(res == LE_FAULT);
    /* Get is possible */
    res = le_mdc_GetAuthentication(ProfileRef[0],&auth, userName, sizeof(userName), password,
                                                                                sizeof(password));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(auth == LE_MDC_AUTH_PAP);
    LE_ASSERT(strcmp(userName, myUserName)==0);
    LE_ASSERT(strcmp(password, myPassword)==0);

    /* Try to set PDP type (error is expected as a connection is in progress) */
    res = le_mdc_SetPDP(ProfileRef[0], LE_MDC_PDP_IPV4V6);
    LE_ASSERT(res == LE_FAULT);
    /* Get is possible */
    pdp = le_mdc_GetPDP(ProfileRef[0]);
    LE_ASSERT(pdp == LE_MDC_PDP_IPV6);

    /* Check that other profiles didn't change */
    for (i = 1; i < NB_PROFILE; i++)
    {
        char tstAPN[10];
        snprintf(tstAPN,10,"TstAPN%d", i);

        /* Check APN */
        res = le_mdc_GetAPN(ProfileRef[i], apn, sizeof(apn));
        LE_ASSERT(res == LE_OK);
        LE_ASSERT(strcmp(tstAPN, apn)==0);

        /* Check auth */
        res = le_mdc_GetAuthentication(ProfileRef[i],&auth, userName, sizeof(userName), password,
                                                                                sizeof(password));
        LE_ASSERT(res == LE_OK);
        LE_ASSERT(auth == LE_MDC_AUTH_NONE);

        /* Check PDP type */
        pdp = le_mdc_GetPDP(ProfileRef[i]);
        LE_ASSERT(pdp == LE_MDC_PDP_IPV4);

        /* Check to get a profile thanks to its APN */
        res = le_mdc_GetProfileFromApn(tstAPN, &profile);
        LE_ASSERT(res == LE_OK);
        LE_ASSERT(profile == ProfileRef[i]);
    }

    /* stop the session */
    res = le_mdc_StopSession(ProfileRef[0]);
    LE_ASSERT(res == LE_OK);

    char homeMcc[]="208";
    char homeMnc[]="01";
    char tstAPN[]="orange";
    pa_simSimu_ReportSimState(LE_SIM_READY);
    pa_simSimu_SetHomeNetworkMccMnc(homeMcc, homeMnc);
    res = le_mdc_SetDefaultAPN(ProfileRef[2]);
    LE_ASSERT(res == LE_OK);
    /* Check APN */
    res = le_mdc_GetAPN(ProfileRef[2], apn, sizeof(apn));
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(strcmp(tstAPN, apn)==0);

    strcpy(homeMcc,"000");
    strcpy(homeMnc,"000");
    pa_simSimu_SetHomeNetworkMccMnc(homeMcc, homeMnc);
    res = le_mdc_SetDefaultAPN(ProfileRef[2]);
    LE_ASSERT(res == LE_FAULT);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function checks if the given profileRef is disconnected by testing IP APIs
 * (used by TestMdc_Connection test).
 *
 * Exit if failed
 */
//--------------------------------------------------------------------------------------------------
static void DisconnectedProfile
(
    le_mdc_ProfileRef_t profileRef
)
{
    char ipAddrStr[IP_STR_SIZE];
    char dns1AddrStr[IP_STR_SIZE];
    char dns2AddrStr[IP_STR_SIZE];
    char gatewayAddrStr[IP_STR_SIZE];
    char interfaceName[10];

    /* Expected value: LE_FAULT as the frofile is supposed to be disconnected */

    LE_ASSERT(le_mdc_GetInterfaceName(profileRef, interfaceName, sizeof(interfaceName))
                                                                                    == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv4Address(profileRef, ipAddrStr, IP_STR_SIZE) == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv6Address(profileRef, ipAddrStr, IP_STR_SIZE) == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv4DNSAddresses(   profileRef,
                                            dns1AddrStr,
                                            IP_STR_SIZE,
                                            dns2AddrStr,
                                            IP_STR_SIZE) == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv6DNSAddresses(   profileRef,
                                            dns1AddrStr,
                                            IP_STR_SIZE,
                                            dns2AddrStr,
                                            IP_STR_SIZE) == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv4GatewayAddress( profileRef, gatewayAddrStr, IP_STR_SIZE )
                                                                                    == LE_FAULT);
    LE_ASSERT(le_mdc_GetIPv6GatewayAddress( profileRef, gatewayAddrStr, IP_STR_SIZE )
                                                                                    == LE_FAULT);

}

//--------------------------------------------------------------------------------------------------
/**
 * The goal of this test is to test the IP adresses APIs (for IPv4, IPv6, IPv4v6)
 * API tested:
 * - le_mdc_GetSessionState
 * - le_mdc_StartSession / le_mdc_StopSession
 * - le_mdc_IsIPv4 / le_mdc_IsIPv6
 * - le_mdc_GetIPv4Address / le_mdc_GetIPv6Address
 * - le_mdc_GetIPv4DNSAddresses / le_mdc_GetIPv6DNSAddresses
 * - le_mdc_GetIPv4GatewayAddress / le_mdc_GetIPv6GatewayAddress
 *
 * Exit if failed
 */
//--------------------------------------------------------------------------------------------------
static void TestMdc_Connection ( void )
{
    int i;
    le_result_t res;
    le_mdc_Pdp_t pdp = LE_MDC_PDP_IPV4;
    char ipAddrStrIpv4[]="192.168.1.100";
    char dns1AddrStrIpv4[]="10.40.50.60.1";
    char dns2AddrStrIpv4[]="10.40.50.60.2";
    char gatewayAddrStrIpv4[]="192.168.100.123";
    char ipAddrStrIpv6[]="2001:0000:3238:DFE1:63::FEFB";
    char dns1AddrStrIpv6[]="2001:4860:4860::8888";
    char dns2AddrStrIpv6[]="2001:4860:4860::8844";
    char gatewayAddrStrIpv6[]="2001:CDBA:0:0:0:0:3257:9652";
    char interfaceName[]="rmnet0";
    char addr[40]="";
    char addr2[40]="";

    /* All profile are disconnected: check connectivities API returns LE_FAULT */
    for (i=0; i < NB_PROFILE; i++)
    {
        DisconnectedProfile(ProfileRef[i]);
    }

    /* Test for all IP type: IPv4, IPv6, IPv4v6 */
    while(pdp <= LE_MDC_PDP_IPV4V6)
    {
        /* check connection status: supposed to be disconnected */
        le_mdc_ConState_t state;
        res = le_mdc_GetSessionState(ProfileRef[0], &state);
        LE_ASSERT(res == LE_OK);
        LE_ASSERT(state == LE_MDC_DISCONNECTED);

        /* Set the new pdp type */
        res = le_mdc_SetPDP(ProfileRef[0], pdp);
        LE_ASSERT(res == LE_OK);

        /* start a session*/
        res = le_mdc_StartSession(ProfileRef[0]);
        LE_ASSERT(res == LE_OK);

        /* Check connection status: supposed to be connected */
        res = le_mdc_GetSessionState(ProfileRef[0], &state);
        LE_ASSERT(res == LE_OK);
        LE_ASSERT(state == LE_MDC_CONNECTED);

        /* check connection status for other profile (supposed to be disconnected) */
        for (i = 1; i < NB_PROFILE; i++)
        {
            res = le_mdc_GetSessionState(ProfileRef[i], &state);
            LE_ASSERT(res == LE_OK);
            LE_ASSERT(state == LE_MDC_DISCONNECTED);

            /* check connectivty parameters */
            DisconnectedProfile(ProfileRef[i]);
        }

        /* configure an interfacen ame in the pa_mdcSimu, and test the API */
        char interfaceNameTmp[20];
        pa_mdcSimu_SetInterfaceName(1, interfaceName);
        LE_ASSERT(le_mdc_GetInterfaceName(ProfileRef[0],interfaceNameTmp,sizeof(interfaceNameTmp))
                                                                                        == LE_OK);
        LE_ASSERT( strcmp(interfaceNameTmp, interfaceName) == 0);

        /* Check IP type */
        switch (pdp)
        {
            case LE_MDC_PDP_IPV4:
                /* configure the pa_mdcSimu with IPv4 addresses */
                pa_mdcSimu_SetIPAddress(1, LE_MDMDEFS_IPV4, ipAddrStrIpv4);
                pa_mdcSimu_SetDNSAddresses(1, LE_MDMDEFS_IPV4, dns1AddrStrIpv4, dns2AddrStrIpv4);
                pa_mdcSimu_SetGatewayAddress(1, LE_MDMDEFS_IPV4, gatewayAddrStrIpv4);

                /* Check IPv4 and IPv6 connectivities:
                 * all IPv6 APIs return an error
                 * IPv4 APIs return expected value
                 */
                LE_ASSERT(le_mdc_IsIPv4(ProfileRef[0]) == true);
                LE_ASSERT(le_mdc_IsIPv6(ProfileRef[0]) == false);

                LE_ASSERT(le_mdc_GetIPv4Address(ProfileRef[0], addr, sizeof(addr) ) == LE_OK);
                LE_ASSERT( strcmp(addr,ipAddrStrIpv4)==0 );
                LE_ASSERT(le_mdc_GetIPv6Address(ProfileRef[0], addr, sizeof(addr) ) == LE_FAULT);
                LE_ASSERT(le_mdc_GetIPv4DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr) ) == LE_OK);
                LE_ASSERT( strcmp(addr,dns1AddrStrIpv4)==0 );
                LE_ASSERT( strcmp(addr2,dns2AddrStrIpv4)==0 );
                LE_ASSERT(le_mdc_GetIPv6DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr2) ) == LE_FAULT);
                LE_ASSERT(le_mdc_GetIPv4GatewayAddress(ProfileRef[0], addr, sizeof(addr)) == LE_OK);
                LE_ASSERT( strcmp(addr,gatewayAddrStrIpv4)==0 );
                LE_ASSERT(le_mdc_GetIPv6GatewayAddress(ProfileRef[0], addr, sizeof(addr)) ==
                                                                                        LE_FAULT);
            break;
            case LE_MDC_PDP_IPV6:
                /* configure the pa_mdcSimu with IPv6 addresses */
                pa_mdcSimu_SetIPAddress(1, LE_MDMDEFS_IPV6, ipAddrStrIpv6);
                pa_mdcSimu_SetDNSAddresses(1, LE_MDMDEFS_IPV6, dns1AddrStrIpv6, dns2AddrStrIpv6);
                pa_mdcSimu_SetGatewayAddress(1, LE_MDMDEFS_IPV6, gatewayAddrStrIpv6);

                /* Check IPv4 and IPv6 connectivities:
                 * all IPv4 APIs return an error
                 * IPv6 APIs return expected value
                 */
                LE_ASSERT(le_mdc_IsIPv4(ProfileRef[0]) == false);
                LE_ASSERT(le_mdc_IsIPv6(ProfileRef[0]) == true);

                LE_ASSERT(le_mdc_GetIPv6Address(ProfileRef[0], addr, sizeof(addr) ) == LE_OK);
                LE_ASSERT( strcmp(addr,ipAddrStrIpv6)==0 );
                LE_ASSERT(le_mdc_GetIPv4Address(ProfileRef[0], addr, sizeof(addr) ) == LE_FAULT);
                LE_ASSERT(le_mdc_GetIPv6DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr2) ) == LE_OK);
                LE_ASSERT( strcmp(addr,dns1AddrStrIpv6)==0 );
                LE_ASSERT( strcmp(addr2,dns2AddrStrIpv6)==0 );
                LE_ASSERT(le_mdc_GetIPv4DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr2) ) == LE_FAULT);
                LE_ASSERT(le_mdc_GetIPv6GatewayAddress(ProfileRef[0], addr, sizeof(addr)) == LE_OK);
                LE_ASSERT( strcmp(addr,gatewayAddrStrIpv6)==0 );
                LE_ASSERT(le_mdc_GetIPv4GatewayAddress(ProfileRef[0], addr, sizeof(addr)) ==
                                                                                        LE_FAULT);
            break;
            case LE_MDC_PDP_IPV4V6:
                /* Check IPv4 and IPv6 connectivities:
                 * IPv4 and IPv6 APIs return expected value
                 */
                LE_ASSERT(le_mdc_IsIPv4(ProfileRef[0]) == true);
                LE_ASSERT(le_mdc_IsIPv6(ProfileRef[0]) == true);

                LE_ASSERT(le_mdc_GetIPv6Address(ProfileRef[0], addr, sizeof(addr) ) == LE_OK);
                LE_ASSERT( strcmp(addr,ipAddrStrIpv6) == 0 );
                LE_ASSERT(le_mdc_GetIPv4Address(ProfileRef[0], addr, sizeof(addr) ) == LE_OK);
                LE_ASSERT( strcmp(addr,ipAddrStrIpv4) == 0 );
                LE_ASSERT(le_mdc_GetIPv6DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr2) ) == LE_OK);
                LE_ASSERT( strcmp(addr,dns1AddrStrIpv6) == 0 );
                LE_ASSERT( strcmp(addr2,dns2AddrStrIpv6) == 0 );
                LE_ASSERT(le_mdc_GetIPv4DNSAddresses(ProfileRef[0], addr,
                                                                    sizeof(addr),
                                                                    addr2,
                                                                    sizeof(addr2) ) == LE_OK);
                LE_ASSERT( strcmp(addr,dns1AddrStrIpv4) == 0 );
                LE_ASSERT( strcmp(addr2,dns2AddrStrIpv4) == 0 );
                LE_ASSERT(le_mdc_GetIPv6GatewayAddress(ProfileRef[0], addr, sizeof(addr)) == LE_OK);
                LE_ASSERT( strcmp(addr,gatewayAddrStrIpv6) == 0 );
                LE_ASSERT(le_mdc_GetIPv4GatewayAddress(ProfileRef[0], addr, sizeof(addr)) ==
                                                                                        LE_OK);
                LE_ASSERT( strcmp(addr,gatewayAddrStrIpv4) == 0 );
            break;
            default:
                LE_ASSERT(0);
            break;
        }

        /* stop the session */
        res = le_mdc_StopSession(ProfileRef[0]);
        LE_ASSERT(res == LE_OK);

        /* next pdp type */
        pdp++;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler of connection: it saves the input parameter in global variables for testing in the main
 * thread.
 * The main thread is waiting for its call using a semaphore, the handler posts a semaphore to
 * unlock it.
 *
 */
//--------------------------------------------------------------------------------------------------
static void HandlerFunc
(
    le_mdc_ProfileRef_t profileRef,
    le_mdc_ConState_t ConnectionStatus,
    void* contextPtr
)
{
    ConnectionStateReceivedByHandler = ( ConnectionStatus == LE_MDC_CONNECTED ) ? true : false;
    ProfileRefReceivedByHandler = (le_mdc_ProfileRef_t) profileRef;

    if (ConnectionStatus == LE_MDC_DISCONNECTED)
    {
        LE_ASSERT( le_mdc_GetDisconnectionReason(profileRef) == LE_MDC_DISC_REGULAR_DEACTIVATION );
        LE_ASSERT( le_mdc_GetPlatformSpecificDisconnectionCode(profileRef) == 2 );
    }

    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove the handler
 * The main thread is waiting for its call using a semaphore, the handler posts a semaphore to
 * unlock it.
 *
 */
//--------------------------------------------------------------------------------------------------
static void RemoveHandler
(
    void* param1Ptr,
    void* param2Ptr
)
{
    le_mdc_RemoveSessionStateHandler(param1Ptr);
    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Thread to test the handler
 * Handlers are called by the event loop, so we need a thread to test the calls.
 * The thread subscribes a handler for each profiles, and then run the event loop.
 * The test is done by the main thread.
 *
 */
//--------------------------------------------------------------------------------------------------
void* ThreadTestHandler( void* ctxPtr )
{

    int i;

    for (i=0; i < NB_PROFILE; i++)
    {
        SessionStateHandler[i] = le_mdc_AddSessionStateHandler (ProfileRef[i],
                                                                HandlerFunc,
                                                                ProfileRef[i]);

        LE_ASSERT(SessionStateHandler[i] != NULL);
    }

    le_sem_Post(ThreadSemaphore);

    le_event_RunLoop();
}

//--------------------------------------------------------------------------------------------------
/**
 * Test the connection handler calls.
 * API tested:
 * - le_mdc_AddSessionStateHandler / le_mdc_RemoveSessionStateHandler
 * - handler called
 *
 */
//--------------------------------------------------------------------------------------------------
void TestMdc_Handler ( void )
{
    le_result_t res;

    /* Create the thread to subcribe and call the handlers */
    ThreadSemaphore = le_sem_Create("HandlerSem",0);
    le_thread_Ref_t thread = le_thread_Create("Threadhandler", ThreadTestHandler, NULL);
    le_thread_Start(thread);
    int i;
    le_clk_Time_t   timeToWait;
    timeToWait.sec = 0;
    timeToWait.usec = 1000000;

    /* Wait the thread to be ready */
    le_sem_Wait(ThreadSemaphore);

    for (i = 0; i < NB_PROFILE; i++)
    {
        /* Start a session for the current profile: the handler should be called */
        res = le_mdc_StartSession(ProfileRef[i]);
        LE_ASSERT(res == LE_OK);
        /* Wait for the call of the handler (error if timeout) */
        LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, timeToWait) == LE_OK);
        /* Check the the handler parameters */
        LE_ASSERT(ProfileRefReceivedByHandler == ProfileRef[i]);
        LE_ASSERT(ConnectionStateReceivedByHandler == true);
        ConnectionStateReceivedByHandler = false;
    }

    for (i = 0; i < NB_PROFILE; i++)
    {
        /* Stop a session for the current profile: the handler should be called */
        res = le_mdc_StopSession(ProfileRef[i]);
        LE_ASSERT(res == LE_OK);
        /* Wait for the call of the handler (error if timeout) */
        LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, timeToWait) == LE_OK);
        /* Check the the handler parameters */
        LE_ASSERT(ProfileRefReceivedByHandler == ProfileRef[i]);
        LE_ASSERT(ConnectionStateReceivedByHandler == false);
        ConnectionStateReceivedByHandler = true;
    }

    /* Remove the handler of the profile 1: ther handler can be removed only by the thread which
     * subscribed to the handler, so put the RemoveHandler() function in queue of this thread */
    le_event_QueueFunctionToThread(         thread,
                                            RemoveHandler,
                                            SessionStateHandler[1],
                                            NULL);
    le_sem_Wait(ThreadSemaphore);
    /* Start a session for the current profile: no handler should be called */
    res = le_mdc_StartSession(ProfileRef[1]);
    /* No semaphore post is waiting, we are expecting a timeout */
    LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, timeToWait) == LE_TIMEOUT);
    res = le_mdc_StopSession(ProfileRef[1]);
    /* No semaphore post is waiting, we are expecting a timeout */
    LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, timeToWait) == LE_TIMEOUT);

}

//--------------------------------------------------------------------------------------------------
/**
 * Test the default profile
 *
 * API tested:
 * - le_mdc_GetProfile with LE_MDC_DEFAULT_PROFILE in argument
 *
 */
//--------------------------------------------------------------------------------------------------
void TestMdc_DefaultProfile
(
    void
)
{
    /* Change the rat to have a CDMA default profile */
    pa_mrcSimu_SetRadioAccessTechInUse(LE_MRC_RAT_CDMA);
    pa_mdc_ProfileData_t profileData;
    memset(&profileData,0,sizeof(pa_mdc_ProfileData_t));

    pa_mdcSimu_SetProfile(PA_MDC_MIN_INDEX_3GPP2_PROFILE, &profileData);

    /* Get the default profile, and check profile index */
    le_mdc_ProfileRef_t profile = le_mdc_GetProfile(LE_MDC_DEFAULT_PROFILE);

    LE_ASSERT(profile!=NULL);
    LE_ASSERT(le_mdc_GetProfileIndex(profile) == PA_MDC_MIN_INDEX_3GPP2_PROFILE);

    pa_mrcSimu_SetRadioAccessTechInUse(LE_MRC_RAT_GSM);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test the data statistics API
 *
 * API tested:
 * - le_mdc_GetBytesCounters
 * - le_mdc_ResetBytesCounter
 *
 */
//--------------------------------------------------------------------------------------------------
void TestMdc_Stat
(
    void
)
{
    pa_mdc_PktStatistics_t dataStatistics;
    dataStatistics.transmittedBytesCount = 123456789;
    dataStatistics.receivedBytesCount = 369258147;

    /* Set the statistics value to the pa */
    pa_mdcSimu_SetDataFlowStatistics(&dataStatistics);

    uint64_t rxBytes;
    uint64_t txBytes;

    /* Get the statistics and check the values */
    le_result_t res = le_mdc_GetBytesCounters(&rxBytes, &txBytes);
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(rxBytes == dataStatistics.receivedBytesCount);
    LE_ASSERT(txBytes == dataStatistics.transmittedBytesCount);

    /* Reset counter, check again statistics (0 values expected) */
    res = le_mdc_ResetBytesCounter();
    LE_ASSERT(res == LE_OK);

    res = le_mdc_GetBytesCounters(&rxBytes, &txBytes);
    LE_ASSERT(res == LE_OK);
    LE_ASSERT(rxBytes == 0);
    LE_ASSERT(txBytes == 0);
}

//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 *
 */
//--------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    LE_LOG_SESSION = log_RegComponent( "mdc", &LE_LOG_LEVEL_FILTER_PTR);

    arg_SetArgs(argc,argv);

    // pa simu init */
    pa_mdcSimu_Init();

    /* init the le_mdc service */
    le_mdc_Init();

    /* init the pa_simu */
    pa_simSimu_Init();

    /* Test configuration */
    TestMdc_Configuration();

    /* Test Connection */
    TestMdc_Connection();

    /* Test handler */
    TestMdc_Handler();

    /* Test default profile */
    TestMdc_DefaultProfile();

    /* Test statistics */
    TestMdc_Stat();

    LE_INFO("Tests MDC passed");

    return 0;
}


