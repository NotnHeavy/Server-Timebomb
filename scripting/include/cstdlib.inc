//////////////////////////////////////////////////////////////////////////////
// MADE BY NOTNHEAVY. USES GPL-3, AS PER REQUEST OF SOURCEMOD               //
//////////////////////////////////////////////////////////////////////////////

// Taken from https://github.com/NotnHeavy/SM-msvcrt-exports.
// I made a few modifications, such as introducing LoadLibrary and GetProcAddress,
// alongside changing how the getPEB subroutine is written in order to call it
// (instead of relying on gamedata and extremely hacky stuff, it just writes
// to an entity instead... still quite hacky).

#if defined NOTNHEAVY_SM_MSVCRT_EXPORTS_CSTDLIB
#endinput
#endif
#define NOTNHEAVY_SM_MSVCRT_EXPORTS_CSTDLIB

#pragma semicolon true 
#pragma newdecls required

#include <sdktools>

#define NULL view_as<any>(0)
#define MAX_PROC_NAME 255

//////////////////////////////////////////////////////////////////////////////
// GLOBALS                                                                  //
//////////////////////////////////////////////////////////////////////////////

enum struct moduledata_t
{
    char name[MAX_PROC_NAME];
    any relativeAddress;
}

enum
{   
    MODULE_MALLOC,
    MODULE_FREE,
    MODULE_MEMMOVE,
    MODULE_MEMCPY,
    MODULE_MEMCMP,
    MODULE_MEMSET,
    MODULE_REALLOC,
    MODULE_CALLOC
}

static char getPEB[] = "\x64\xA1\x30\x00\x00\x00\xC3"; // mov eax, [fs:0x30]
                                                       // ret

static moduledata_t functions[] = { {"malloc"}, {"free"}, {"memmove"}, {"memcpy"}, {"memcmp"}, {"memset"}, {"realloc"}, {"calloc"} };

static Handle SDKCall_malloc;
static Handle SDKCall_free;
static Handle SDKCall_memmove;
static Handle SDKCall_memcpy;
static Handle SDKCall_memcmp;
static Handle SDKCall_memset;
static Handle SDKCall_realloc;
static Handle SDKCall_calloc;

//////////////////////////////////////////////////////////////////////////////
// STOCKS                                                                   //
//////////////////////////////////////////////////////////////////////////////

// The Process Environment Block (PEB) object for this process.
stock any g_PEB = NULL;

// Returns a wide string as a C string.
stock void WideStringToCString(any memory, char[] buffer, int length, bool wide = false)
{
    for (int i = 0; i < length - 1; ++i)
    {
        char value = LoadFromAddress(memory + i * (wide ? 2 : 1), NumberType_Int8);
        buffer[i] = value;
        if (!value)
            break;
    }
    buffer[length - 1] = '\x0';
}

// Get the image size from the specified image base.
stock any GetImageSize(const any base)
{
    // Verify that this is the DOS header.
    if (LoadFromAddress(base, NumberType_Int16) != 0x5A4D)
        return 0;
    return LoadFromAddress(base + LoadFromAddress(base + 0x3C, NumberType_Int32) + 0x50, NumberType_Int32); // Get offset to NT header from DOS header, then retrieve PIMAGE_OPTIONAL_HEADER32->SizeOfImage;
}

// Return the address to data from the image base of a library, with a specified pattern. Leave imageSize as 0 to use GetImageSize().
// Note that this is slow.
stock any FindPattern(any base, const char[] signature, const int length, const int imageSize = 0)
{
    any current = base;
    int i = 0;
    int end = base + (imageSize ? imageSize : GetImageSize(base));
    while (current < end)
    {
        for (i = 0; i < length; ++i)
        {
            if (signature[i] != '\x2A' && signature[i] != LoadFromAddress(current + i, NumberType_Int8))
                break;
        }
        if (i == length)
            return current;
        ++current;
    }
    return 0;
}

// Allocate space for a new subroutine within this image.
// You should resort to using malloc() or getting the address of a string instead.
stock any AllocateSubroutineOnImage(const any base, const char[] subroutine, const int length)
{
    any address = base + GetImageSize(base) - 1;
    while (address > base)
    {
        if (LoadFromAddress(address, NumberType_Int8) != 0)
            break;
        --address;
    }
    address = (address + 0x10) & 0xFFFFFFF0;
    for (int i = 0; i < length; ++i)
        StoreToAddress(address + i, subroutine[i], NumberType_Int8);
    return address;
}

// Return a loaded module using 8-bit chars, NULL if not present.
stock any LoadLibrary(char[] module)
{
    // Go to the beginning of the module list.
    any address = LoadFromAddress(g_PEB + 0xC, NumberType_Int32); // peb->ldr
    address = LoadFromAddress(address + 0x14, NumberType_Int32); // ldr->InMemoryOrderModuleList;
    any initialModule = address;

    // Walk through all modules loaded and find the one we'd like to retrieve.
    for (;;)
    {
        // Get the actual starting data table entry, then get some information about the module.
        any _LDR_DATA_TABLE_ENTRY = address - 0x8;
        any BaseDllName = LoadFromAddress(_LDR_DATA_TABLE_ENTRY + 0x2C + 0x4, NumberType_Int32); // dereference BaseDllName wstring pointer.
        any DllBase = LoadFromAddress(_LDR_DATA_TABLE_ENTRY + 0x18, NumberType_Int32); // dereference DllBase to get the image base of the module.

        // Obtain the base DLL name.
        // The structure used for the name variable follows as so:
        /*
        typedef struct _UNICODE_STRING 
        {
            USHORT Length;
            USHORT MaximumLength;
            PWSTR  Buffer;
        } UNICODE_STRING, *PUNICODE_STRING;
        */
        // Essentially the actual buffer is + 0x4 from the actual variable address. The string is also a wide string, meaning each character is 2 bytes.
        // We will only be needing the wide chars as normal chars.
        char buffer[64];
        WideStringToCString(BaseDllName, buffer, sizeof(buffer), true);

        // Check if these are any of the modules we want.
        if (strcmp(buffer, module) == 0)
            return DllBase;
        
        // Get the next module in the list. End the loop if we have obtained both module image bases. If this is the end of the chain, return NULL.
        address = LoadFromAddress(address, NumberType_Int32);
        if (LoadFromAddress(address, NumberType_Int32) == initialModule)
            return NULL;
    }
}

// Return an exported function within a module, NULL if not present.
stock any GetProcAddress(any module, char[] proc)
{
    // Retrieve the exports table.
    any msvcrtExportTable = LoadFromAddress(module + LoadFromAddress(module + 0x3C, NumberType_Int32) + 0x78, NumberType_Int32) + module; // module->ntheader->export_table;

    // Locate the procedure by comparing it to the name pointer table and locating its offset to the address table.
    any msvcrtNumberOfFunctions = LoadFromAddress(msvcrtExportTable + 0x14, NumberType_Int32);
    any msvcrtAddressTable = LoadFromAddress(msvcrtExportTable + 0x1C, NumberType_Int32) + module;
    any msvcrtNamePointerTable = LoadFromAddress(msvcrtExportTable + 0x20, NumberType_Int32) + module;
    any msvcrtOrdinalTable = LoadFromAddress(msvcrtExportTable + 0x24, NumberType_Int32) + module;
    for (int i = 0; i < msvcrtNumberOfFunctions; ++i)
    {
        char buffer[MAX_PROC_NAME];
        WideStringToCString(LoadFromAddress(msvcrtNamePointerTable + i * 4, NumberType_Int32) + module, buffer, sizeof(buffer));
        if (strcmp(buffer, proc) == 0)
        {
            int ordinal = LoadFromAddress(msvcrtOrdinalTable + i * 2, NumberType_Int16);
            return module + LoadFromAddress(msvcrtAddressTable + ordinal * 4, NumberType_Int32);
        }
    }

    // Procedure not found; return NULL.
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
// cstdlib()                                                                //
//////////////////////////////////////////////////////////////////////////////

// This loads the application's PEB and gives you a few memory functions that you can work with
// by walking through msvcrt.dll's exports table.
stock void cstdlib()
{
    if (g_PEB != NULL)
        return;

    // Obtain this application's PEB.
    char path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), "");
    if (path[strlen(path) - 1] == '/')
    {
        PrintToServer("This plugin only works on Windows!");
        return;
    }
    
    int light = CreateEntityByName("light");
    any lightAddress = GetEntityAddress(light) + view_as<any>(FindDataMapInfo(light, "m_flSpeed"));
    for (int i = 0; i < sizeof(getPEB) - 1; ++i)
        StoreToAddress(lightAddress + i, getPEB[i], NumberType_Int8);

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(lightAddress);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    Handle SDKCall_GetPEB = EndPrepSDKCall();
    g_PEB = SDKCall(SDKCall_GetPEB);
    delete SDKCall_GetPEB;
    
    RemoveEntity(light);

    // Retrieve the msvcrt.dll module.
    any stdlibBase = LoadLibrary("msvcrt.dll");
    if (stdlibBase == NULL)
        ThrowError("Could not find msvcrt.dll.");

    // After retrieving the image base for msvcrt.dll, peak through the exports table to retrieve our
    // wanted memory functions.name
    any msvcrtExportTable = stdlibBase;
    msvcrtExportTable = LoadFromAddress(msvcrtExportTable + LoadFromAddress(msvcrtExportTable + 0x3C, NumberType_Int32) + 0x78, NumberType_Int32) + stdlibBase; // msvcrt->ntheader->export_table;

    // Get some information about the export table and fill in our module information table.
    any msvcrtNumberOfFunctions = LoadFromAddress(msvcrtExportTable + 0x14, NumberType_Int32);
    any msvcrtAddressTable = LoadFromAddress(msvcrtExportTable + 0x1C, NumberType_Int32) + stdlibBase;
    any msvcrtNamePointerTable = LoadFromAddress(msvcrtExportTable + 0x20, NumberType_Int32) + stdlibBase;
    any msvcrtOrdinalTable = LoadFromAddress(msvcrtExportTable + 0x24, NumberType_Int32) + stdlibBase;
    for (int i = 0; i < msvcrtNumberOfFunctions; ++i)
    {
        char buffer[MAX_PROC_NAME];
        WideStringToCString(LoadFromAddress(msvcrtNamePointerTable + i * 4, NumberType_Int32) + stdlibBase, buffer, sizeof(buffer));
        for (int y = 0; y < sizeof(functions); ++y)
        {
            if (strcmp(buffer, functions[y].name) == 0)
            {
                int ordinal = LoadFromAddress(msvcrtOrdinalTable + i * 2, NumberType_Int16);
                functions[y].relativeAddress = LoadFromAddress(msvcrtAddressTable + ordinal * 4, NumberType_Int32);
                break;
            }
        }
    }

    // Now we can start creating our SDKCalls with all the relative addresses we have. :^)
    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_MALLOC].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_malloc = EndPrepSDKCall();

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_FREE].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_free = EndPrepSDKCall();

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_MEMMOVE].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_memmove = EndPrepSDKCall();

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_MEMCPY].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_memcpy = EndPrepSDKCall();
    
    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_MEMCMP].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_memcmp = EndPrepSDKCall();

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_MEMSET].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_memset = EndPrepSDKCall();
    
    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_REALLOC].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_realloc = EndPrepSDKCall();

    StartPrepSDKCall(SDKCall_Static);
    PrepSDKCall_SetAddress(stdlibBase + functions[MODULE_CALLOC].relativeAddress);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
    PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
    SDKCall_calloc = EndPrepSDKCall();
}

//////////////////////////////////////////////////////////////////////////////
// stdlib                                                                   //
//////////////////////////////////////////////////////////////////////////////

stock any malloc(int size)
{
    return SDKCall(SDKCall_malloc, size);
}

stock any calloc(int num, int size)
{
    return SDKCall(SDKCall_calloc, num, size);
}

stock any realloc(any ptr, int size)
{
    return SDKCall(SDKCall_realloc, ptr, size);
}

stock any reallocf(any ptr, int size)
{
    return SDKCall(SDKCall_realloc, ptr, size);
}

stock void free(any ptr)
{
    SDKCall(SDKCall_free, ptr);
}

stock void freef(any ptr)
{
    SDKCall(SDKCall_free, ptr);
}

stock any memmove(any dest, any src, int count)
{
    return SDKCall(SDKCall_memmove, dest, src, count);
}

stock any memmovef(any dest, any src, int count)
{
    return SDKCall(SDKCall_memmove, dest, src, count);
}

stock any memcpy(any dest, any src, int count)
{
    return SDKCall(SDKCall_memcpy, dest, src, count);
}

stock any memcpyf(any dest, any src, int count)
{
    return SDKCall(SDKCall_memcpy, dest, src, count);
}

stock int memcmp(any lhs, any rhs, int count)
{
    return SDKCall(SDKCall_memcmp, lhs, rhs, count);
}

stock int memcmpf(any lhs, any rhs, int count)
{
    return SDKCall(SDKCall_memcmp, lhs, rhs, count);
}

stock any memset(any dest, int val, int count)
{
    return SDKCall(SDKCall_memset, dest, val, count);
}

stock any memsetf(any dest, int val, int count)
{
    return SDKCall(SDKCall_memset, dest, val, count);
}