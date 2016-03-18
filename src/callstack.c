
#include "functions.h"

#if defined(MSWIN)
# include <dbghelp.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Version.lib")
#endif

// Normally it should be enough to use 'CONTEXT_FULL'
// (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

#define STACKWALK_MAX_NAMELEN	1024

#if defined(_M_IX86) && defined(_MSC_VER)
#ifdef CURRENT_THREAD_VIA_EXCEPTION
// TODO: The following is not a "good" implementation,
// because the callstack is only valid in the "__except" block...
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    EXCEPTION_POINTERS *pExp = NULL; \
    __try { \
      throw 0; \
        } __except( ( (pExp = GetExceptionInformation()) ? \
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_EXECUTE_HANDLER)) {} \
    if (pExp != NULL) \
      memcpy(&c, pExp->ContextRecord, sizeof(CONTEXT)); \
      c.ContextFlags = contextFlags; \
    } while(0);
#else
// The following should be enough for walking the callstack...
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    c.ContextFlags = contextFlags; \
    __asm    call x \
    __asm x: pop eax \
    __asm    mov c.Eip, eax \
    __asm    mov c.Ebp, ebp \
    __asm    mov c.Esp, esp \
    } while(0);
#endif

#else

// The following is defined for x86 (XP and higher), x64 and IA64:
#define GET_CURRENT_CONTEXT(c, contextFlags) \
  do { \
    memset(&c, 0, sizeof(CONTEXT)); \
    c.ContextFlags = contextFlags; \
    RtlCaptureContext(&c); \
  } while(0);
#endif

typedef struct IMAGEHLP_MODULE64_V2 {
    DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64  BaseOfImage;            // base load address of module
    DWORD    ImageSize;              // virtual size of the loaded module
    DWORD    TimeDateStamp;          // date/time stamp from pe header
    DWORD    CheckSum;               // checksum from the pe header
    DWORD    NumSyms;                // number of symbols in the symbol table
    SYM_TYPE SymType;                // type of symbols loaded
    CHAR     ModuleName[32];         // module name
    CHAR     ImageName[256];         // image name
    CHAR     LoadedImageName[256];   // symbol file name
} IMAGEHLP_MODULE64_V2;

static HMODULE g_cmutil_hDbhHelp = NULL;

// SymCleanup()
typedef BOOL(__stdcall *tSC)(IN HANDLE hProcess);
static tSC pSC;

// SymFunctionTableAccess64()
typedef PVOID(__stdcall *tSFTA)(HANDLE hProcess, DWORD64 AddrBase);
static tSFTA pSFTA;

// SymGetLineFromAddr64()
typedef BOOL(__stdcall *tSGLFA)(IN HANDLE hProcess, IN DWORD64 dwAddr,
    OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line);
static tSGLFA pSGLFA;

// SymGetModuleBase64()
typedef DWORD64(__stdcall *tSGMB)(IN HANDLE hProcess, IN DWORD64 dwAddr);
static tSGMB pSGMB;

// SymGetModuleInfo64()
typedef BOOL(__stdcall *tSGMI)(IN HANDLE hProcess, IN DWORD64 dwAddr,
    OUT IMAGEHLP_MODULE64_V2 *ModuleInfo);
static tSGMI pSGMI;

// SymGetOptions()
typedef DWORD(__stdcall *tSGO)(VOID);
static tSGO pSGO;

// SymGetSymFromAddr64()
typedef BOOL(__stdcall *tSGSFA)(IN HANDLE hProcess, IN DWORD64 dwAddr,
    OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol);
static tSGSFA pSGSFA;

// SymInitialize()
typedef BOOL(__stdcall *tSI)(IN HANDLE hProcess, IN PSTR UserSearchPath,
    IN BOOL fInvadeProcess);
static tSI pSI;

// SymLoadModule64()
typedef DWORD64(__stdcall *tSLM)(IN HANDLE hProcess, IN HANDLE hFile,
    IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll,
    IN DWORD SizeOfDll);
static tSLM pSLM;

// SymSetOptions()
typedef DWORD(__stdcall *tSSO)(IN DWORD SymOptions);
static tSSO pSSO;

// StackWalk64()
typedef BOOL(__stdcall *tSW)(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
static tSW pSW;

// UnDecorateSymbolName()
typedef DWORD(__stdcall WINAPI *tUDSN)(
    PCSTR DecoratedName, PSTR UnDecoratedName,
    DWORD UndecoratedLength, DWORD Flags);
static tUDSN pUDSN;

typedef BOOL(__stdcall WINAPI *tSGSP)(
    HANDLE hProcess, PSTR SearchPath, DWORD SearchPathLength);
static tSGSP pSGSP;

static HANDLE hProcess;
static DWORD dwProcessId;

void CMUTIL_CallStackInit()
{
    char szTemp[4096] = { 0, };
    // But before wqe do this, we first check if the ".local" file exists
    if (GetModuleFileName(NULL, szTemp, 4096) > 0)
    {
        strcat(szTemp, ".local");
        if (GetFileAttributes(szTemp) == INVALID_FILE_ATTRIBUTES)
        {
            // ".local" file does not exist, so we can try to load
            // the dbghelp.dll from the "Debugging Tools for Windows"
            if (GetEnvironmentVariable("ProgramFiles", szTemp, 4096) > 0)
            {
                strcat(szTemp, "\\Debugging Tools for Windows\\dbghelp.dll");
                // now check if the file exists:
                if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
                {
                    g_cmutil_hDbhHelp = LoadLibrary(szTemp);
                }
            }
            // Still not found? Then try to load the 64-Bit version:
            if ((g_cmutil_hDbhHelp == NULL) &&
                (GetEnvironmentVariable("ProgramFiles", szTemp, 4096) > 0))
            {
                strcat(szTemp,
                    "\\Debugging Tools for Windows 64-Bit\\dbghelp.dll");
                if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
                {
                    g_cmutil_hDbhHelp = LoadLibrary(szTemp);
                }
            }
        }
    }
    if (g_cmutil_hDbhHelp == NULL)
        g_cmutil_hDbhHelp = LoadLibrary("dbghelp.dll");
    if (g_cmutil_hDbhHelp == NULL) {
        // TODO: report error
    }
    else {
        dwProcessId = GetCurrentProcessId();
        hProcess = GetCurrentProcess();

        pSI = (tSI)GetProcAddress(
            g_cmutil_hDbhHelp, "SymInitialize");
        pSC = (tSC)GetProcAddress(
            g_cmutil_hDbhHelp, "SymCleanup");

        pSW = (tSW)GetProcAddress(
            g_cmutil_hDbhHelp, "StackWalk64");
        pSGO = (tSGO)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetOptions");
        pSSO = (tSSO)GetProcAddress(
            g_cmutil_hDbhHelp, "SymSetOptions");

        pSFTA = (tSFTA)GetProcAddress(
            g_cmutil_hDbhHelp, "SymFunctionTableAccess64");
        pSGLFA = (tSGLFA)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetLineFromAddr64");
        pSGMB = (tSGMB)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetModuleBase64");
        pSGMI = (tSGMI)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetModuleInfo64");
        pSGSFA = (tSGSFA)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetSymFromAddr64");
        pUDSN = (tUDSN)GetProcAddress(
            g_cmutil_hDbhHelp, "UnDecorateSymbolName");
        pSLM = (tSLM)GetProcAddress(
            g_cmutil_hDbhHelp, "SymLoadModule64");
        pSGSP = (tSGSP)GetProcAddress(
            g_cmutil_hDbhHelp, "SymGetSearchPath");

        if (pSI(hProcess, NULL, FALSE) == FALSE) {
            // TODO: report error
        }

        DWORD symOptions = pSGO();  // SymGetOptions
        symOptions |= SYMOPT_LOAD_LINES;
        symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
        //symOptions |= SYMOPT_NO_PROMPTS;
        // SymSetOptions
        symOptions = pSSO(symOptions);
    }
}

void CMUTIL_CallStackClear()
{
    if (pSC != NULL && hProcess != NULL)
        pSC(hProcess);
    hProcess = NULL;
    if (g_cmutil_hDbhHelp)
        FreeLibrary(g_cmutil_hDbhHelp);
    g_cmutil_hDbhHelp = NULL;
}

// **************************************** ToolHelp32 ************************
#define MAX_MODULE_NAME32	255
#define TH32CS_SNAPMODULE   0x00000008
#pragma pack( push, 8 )
typedef struct tagMODULEENTRY32
{
    DWORD   dwSize;
    DWORD   th32ModuleID;       // This module
    DWORD   th32ProcessID;      // owning process
    DWORD   GlblcntUsage;       // Global usage count on the module
    DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
    BYTE  * modBaseAddr;        // Base address of module in th32ProcessID's
                                //  context
    DWORD   modBaseSize;        // Size in bytes of module starting at
                                //  modBaseAddr
    HMODULE hModule;            // The hModule of this module in
                                //  th32ProcessID's context
    char    szModule[MAX_MODULE_NAME32 + 1];
    char    szExePath[MAX_PATH];
} MODULEENTRY32;
typedef MODULEENTRY32 *  PMODULEENTRY32;
typedef MODULEENTRY32 *  LPMODULEENTRY32;
#pragma pack( pop )

// **************************************** PSAPI ************************
typedef struct _MODULEINFO {
    LPVOID lpBaseOfDll;
    DWORD SizeOfImage;
    LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;

typedef struct CMUTIL_StackWalker_Internal CMUTIL_StackWalker_Internal;
struct CMUTIL_StackWalker_Internal {
    CMUTIL_StackWalker	base;
    DWORD (*LoadModule)(
        CMUTIL_StackWalker_Internal *walker,
        LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size);
    BOOL (*GetModuleListTH32)(
        CMUTIL_StackWalker_Internal *walker);
    BOOL (*GetModuleListPSAPI)(
        CMUTIL_StackWalker_Internal *walker);
    BOOL (*LoadModules)(
        CMUTIL_StackWalker_Internal *walker);
    BOOL (*GetModuleInfo)(
        CMUTIL_StackWalker_Internal *walker, DWORD64 baseAddr,
        IMAGEHLP_MODULE64_V2 *pModuleInfo);
    CMUTIL_Mem_st *memst;
};

CMUTIL_STATIC BOOL CMUTIL_StackWalkerGetModuleListTH32(
    CMUTIL_StackWalker_Internal *walker)
{
    // CreateToolhelp32Snapshot()
    typedef HANDLE(__stdcall *tCT32S)(DWORD dwFlags, DWORD th32ProcessID);
    // Module32First()
    typedef BOOL(__stdcall *tM32F)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
    // Module32Next()
    typedef BOOL(__stdcall *tM32N)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

    // try both dlls...
    const char *dllname[] = { "kernel32.dll", "tlhelp32.dll" };
    HINSTANCE hToolhelp = NULL;
    tCT32S pCT32S = NULL;
    tM32F pM32F = NULL;
    tM32N pM32N = NULL;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    BOOL keepGoing;
    size_t i;

    for (i = 0; i<(sizeof(dllname) / sizeof(dllname[0])); i++)
    {
        hToolhelp = LoadLibrary(dllname[i]);
        if (hToolhelp == NULL)
            continue;
        pCT32S = (tCT32S)GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
        pM32F = (tM32F)GetProcAddress(hToolhelp, "Module32First");
        pM32N = (tM32N)GetProcAddress(hToolhelp, "Module32Next");
        if ((pCT32S != NULL) && (pM32F != NULL) && (pM32N != NULL))
            break; // found the functions!
        FreeLibrary(hToolhelp);
        hToolhelp = NULL;
    }

    if (hToolhelp == NULL)
        return FALSE;

    hSnap = pCT32S(TH32CS_SNAPMODULE, dwProcessId);
    if (hSnap == (HANDLE)-1)
        return FALSE;

    keepGoing = !!pM32F(hSnap, &me);
    int cnt = 0;
    while (keepGoing)
    {
        CMUTIL_CALL(walker, LoadModule, me.szExePath, me.szModule,
            (DWORD64)me.modBaseAddr, me.modBaseSize);
        cnt++;
        keepGoing = !!pM32N(hSnap, &me);
    }
    CloseHandle(hSnap);
    FreeLibrary(hToolhelp);
    if (cnt <= 0)
        return FALSE;
    return TRUE;
}  // GetModuleListTH32

CMUTIL_STATIC BOOL CMUTIL_StackWalkerGetModuleListPSAPI(
    CMUTIL_StackWalker_Internal *walker)
{
    // EnumProcessModules()
    typedef BOOL(__stdcall *tEPM)(
        HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);
    // GetModuleFileNameEx()
    typedef DWORD(__stdcall *tGMFNE)(
        HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
    // GetModuleBaseName()
    typedef DWORD(__stdcall *tGMBN)(
        HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
    // GetModuleInformation()
    typedef BOOL(__stdcall *tGMI)(
        HANDLE hProcess, HMODULE hModule, LPMODULEINFO pmi, DWORD nSize);

    HINSTANCE hPsapi;
    tEPM pEPM;
    tGMFNE pGMFNE;
    tGMBN pGMBN;
    tGMI pGMI;

    DWORD i;
    //ModuleEntry e;
    DWORD cbNeeded;
    MODULEINFO mi;
    HMODULE *hMods = 0;
    char *tt = NULL;
    char *tt2 = NULL;
    const SIZE_T TTBUFLEN = 8096;
    int cnt = 0;

    hPsapi = LoadLibrary("psapi.dll");
    if (hPsapi == NULL)
        return FALSE;

    pEPM = (tEPM)GetProcAddress(hPsapi, "EnumProcessModules");
    pGMFNE = (tGMFNE)GetProcAddress(hPsapi, "GetModuleFileNameExA");
    pGMBN = (tGMFNE)GetProcAddress(hPsapi, "GetModuleBaseNameA");
    pGMI = (tGMI)GetProcAddress(hPsapi, "GetModuleInformation");
    if ((pEPM == NULL) || (pGMFNE == NULL) || (pGMBN == NULL) || (pGMI == NULL))
    {
        // we couldn't find all functions
        FreeLibrary(hPsapi);
        return FALSE;
    }

    hMods = (HMODULE*)walker->memst->Alloc(
                sizeof(HMODULE) * (TTBUFLEN / sizeof(HMODULE)));
    tt = (char*)walker->memst->Alloc(sizeof(char) * TTBUFLEN);
    tt2 = (char*)walker->memst->Alloc(sizeof(char) * TTBUFLEN);
    if ((hMods == NULL) || (tt == NULL) || (tt2 == NULL))
        goto cleanup;

    if (!pEPM(hProcess, hMods, (DWORD)TTBUFLEN, &cbNeeded))
    {
        goto cleanup;
    }

    if (cbNeeded > TTBUFLEN)
    {
        goto cleanup;
    }

    for (i = 0; i < cbNeeded / sizeof hMods[0]; i++)
    {
        // base address, size
        pGMI(hProcess, hMods[i], &mi, sizeof mi);
        // image file name
        tt[0] = 0;
        pGMFNE(hProcess, hMods[i], tt, (DWORD)TTBUFLEN);
        // module name
        tt2[0] = 0;
        pGMBN(hProcess, hMods[i], tt2, (DWORD)TTBUFLEN);

        DWORD dwRes = CMUTIL_CALL(walker, LoadModule, tt, tt2,
            (DWORD64)mi.lpBaseOfDll, mi.SizeOfImage);
        if (dwRes != ERROR_SUCCESS) {
            // TODO: report error
        }
        cnt++;
    }

cleanup:
    if (hPsapi != NULL) FreeLibrary(hPsapi);
    if (tt2 != NULL) walker->memst->Free(tt2);
    if (tt != NULL) walker->memst->Free(tt);
    if (hMods != NULL) walker->memst->Free(hMods);

    return cnt != 0? TRUE:FALSE;
}  // GetModuleListPSAPI

CMUTIL_STATIC DWORD CMUTIL_StackWalkerLoadModule(
    CMUTIL_StackWalker_Internal *walker,
    LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size)
{
    CHAR *szImg = walker->memst->Strdup(img);
    CHAR *szMod = walker->memst->Strdup(mod);
    DWORD result = ERROR_SUCCESS;
    if ((szImg == NULL) || (szMod == NULL))
        result = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
        if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size) == 0)
            result = GetLastError();
    }
    if (szImg != NULL) walker->memst->Free(szImg);
    if (szMod != NULL) walker->memst->Free(szMod);
    return result;
}

CMUTIL_STATIC BOOL CMUTIL_StackWalkerLoadModules(
    CMUTIL_StackWalker_Internal *walker)
{
    // first try toolhelp32
    if (CMUTIL_CALL(walker, GetModuleListTH32))
        return TRUE;
    // then try psapi
    return CMUTIL_CALL(walker, GetModuleListPSAPI);
}

CMUTIL_STATIC BOOL CMUTIL_StackWalkerGetModuleInfo(
    CMUTIL_StackWalker_Internal *walker, DWORD64 baseAddr,
    IMAGEHLP_MODULE64_V2 *pModuleInfo)
{
    if (pSGMI == NULL)
    {
        SetLastError(ERROR_DLL_INIT_FAILED);
        return FALSE;
    }
    // First try to use the larger ModuleInfo-Structure
    //    memset(pModuleInfo, 0, sizeof(IMAGEHLP_MODULE64_V3));
    //    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V3);
    //    if (this->pSGMI_V3 != NULL)
    //    {
    //      if (this->pSGMI_V3(hProcess, baseAddr, pModuleInfo) != FALSE)
    //        return TRUE;
    //      // check if the parameter was wrong (size is bad...)
    //      if (GetLastError() != ERROR_INVALID_PARAMETER)
    //        return FALSE;
    //    }
    // could not retrive the bigger structure, try with the smaller one
    // (as defined in VC7.1)...
    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
    // reserve enough memory, so the bug in v6.3.5.1 does not lead to
    // memory-overwrites...
    void *pData = walker->memst->Alloc(4096);
    if (pData == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULE64_V2));
    if (pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULE64_V2*)pData) != FALSE)
    {
        // only copy as much memory as is reserved...
        memcpy(pModuleInfo, pData, sizeof(IMAGEHLP_MODULE64_V2));
        pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
        walker->memst->Free(pData);
        return TRUE;
    }
    walker->memst->Free(pData);
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
}

typedef enum CallstackEntryType {
    firstEntry, nextEntry, lastEntry
} CallstackEntryType;

typedef struct CallstackEntry
{
    DWORD64 offset;  // if 0, we have no valid entry
    CHAR name[STACKWALK_MAX_NAMELEN];
    CHAR undName[STACKWALK_MAX_NAMELEN];
    CHAR undFullName[STACKWALK_MAX_NAMELEN];
    DWORD64 offsetFromSmybol;
    DWORD offsetFromLine;
    DWORD lineNumber;
    CHAR lineFileName[STACKWALK_MAX_NAMELEN];
    DWORD symType;
    LPCSTR symTypeString;
    CHAR moduleName[STACKWALK_MAX_NAMELEN];
    DWORD64 baseOfImage;
    CHAR loadedImageName[STACKWALK_MAX_NAMELEN];
} CallstackEntry;

typedef void (*CMUTIL_StackWalkerStackEntryReader)(
    const char *stackinfo, int len, void * userdat);

CMUTIL_STATIC BOOL __stdcall CMUTIL_StackWalkerReadProcMem(
    HANDLE      hProcess,
    DWORD64     qwBaseAddress,
    PVOID       lpBuffer,
    DWORD       nSize,
    LPDWORD     lpNumberOfBytesRead
    )
{
    SIZE_T st;
    BOOL bRet = ReadProcessMemory(
        hProcess, (LPVOID)qwBaseAddress, lpBuffer, nSize, &st);
    *lpNumberOfBytesRead = (DWORD)st;
    //printf("ReadMemory: hProcess: %p, baseAddr: %p, buffer: %p, size: %d, "
    //"read: %d, result: %d\n", hProcess, (LPVOID) qwBaseAddress, lpBuffer,
    //nSize, (DWORD) st, (DWORD) bRet);
    return bRet;
}

CMUTIL_STATIC BOOL CMUTIL_StackWalkerShowCallstack(
    CMUTIL_StackWalker_Internal *walker,
    CMUTIL_StackWalkerStackEntryReader readMemoryFunction, LPVOID pUserData)
{
    CONTEXT c;;
    CallstackEntry csEntry;
    IMAGEHLP_SYMBOL64 *pSym = NULL;
    IMAGEHLP_MODULE64_V2 Module;
    IMAGEHLP_LINE64 Line;
    int frameNum;
    HANDLE hThread = GetCurrentThread();

    // modules are loaded already
    //if (walker->m_modulesLoaded == FALSE)
    CMUTIL_CALL(walker, LoadModules);  // ignore the result...

    GET_CURRENT_CONTEXT(c, USED_CONTEXT_FLAGS);

    // init STACKFRAME for first call
    STACKFRAME64 s; // in/out stackframe
    memset(&s, 0, sizeof(s));
    DWORD imageType;
#ifdef _M_IX86
    // normally, call ImageNtHeader() and use machine info from PE header
    imageType = IMAGE_FILE_MACHINE_I386;
    s.AddrPC.Offset = c.Eip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.Ebp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.Esp;
    s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    imageType = IMAGE_FILE_MACHINE_AMD64;
    s.AddrPC.Offset = c.Rip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.Rsp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.Rsp;
    s.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    imageType = IMAGE_FILE_MACHINE_IA64;
    s.AddrPC.Offset = c.StIIP;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.IntSp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrBStore.Offset = c.RsBSP;
    s.AddrBStore.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.IntSp;
    s.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

    pSym = (IMAGEHLP_SYMBOL64 *)walker->memst->Alloc(
        sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
    if (!pSym) goto cleanup;  // not enough memory...
    memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
    pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;

    memset(&Line, 0, sizeof(Line));
    Line.SizeOfStruct = sizeof(Line);

    memset(&Module, 0, sizeof(Module));
    Module.SizeOfStruct = sizeof(Module);

    for (frameNum = 0;; ++frameNum)
    {
        // get next stack frame (StackWalk64(), SymFunctionTableAccess64(),
        // SymGetModuleBase64()) if this returns ERROR_INVALID_ADDRESS (487) or
        // ERROR_NOACCESS (998), you can assume that either you are done, or
        // that the stack is so hosed that the next deeper frame could not be
        // found. CONTEXT need not to be suplied if imageTyp is
        // IMAGE_FILE_MACHINE_I386!
        if (!pSW(imageType, hProcess, hThread, &s, &c,
            CMUTIL_StackWalkerReadProcMem, pSFTA, pSGMB, NULL))
        {
            // TODO: error report
            //this->OnDbgHelpErr("StackWalk64", GetLastError(), s.AddrPC.Offset);
            break;
        }

        csEntry.offset = s.AddrPC.Offset;
        csEntry.name[0] = 0;
        csEntry.undName[0] = 0;
        csEntry.undFullName[0] = 0;
        csEntry.offsetFromSmybol = 0;
        csEntry.offsetFromLine = 0;
        csEntry.lineFileName[0] = 0;
        csEntry.lineNumber = 0;
        csEntry.loadedImageName[0] = 0;
        csEntry.moduleName[0] = 0;
        if (s.AddrPC.Offset == s.AddrReturn.Offset)
        {
            // TODO: error report
            //this->OnDbgHelpErr("StackWalk64-Endless-Callstack!", 0,
            //s.AddrPC.Offset);
            break;
        }
        if (s.AddrPC.Offset != 0)
        {
            // we seem to have a valid PC
            // show procedure info (SymGetSymFromAddr64())
            if (pSGSFA(hProcess, s.AddrPC.Offset, &(csEntry.offsetFromSmybol),
                pSym) != FALSE)
            {
                // TODO: Mache dies sicher...!
                strcpy(csEntry.name, pSym->Name);
                // UnDecorateSymbolName()
                pUDSN(pSym->Name, csEntry.undName,
                    STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY);
                pUDSN(pSym->Name, csEntry.undFullName,
                    STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE);
            }
            else
            {
                // TODO: error report
                //this->OnDbgHelpErr("SymGetSymFromAddr64", GetLastError(),
                //s.AddrPC.Offset);
            }

            // show line number info, NT5.0-method (SymGetLineFromAddr64())
            if (pSGLFA != NULL)
            { // yes, we have SymGetLineFromAddr64()
                if (pSGLFA(hProcess, s.AddrPC.Offset,
                    &(csEntry.offsetFromLine), &Line) != FALSE)
                {
                    csEntry.lineNumber = Line.LineNumber;
                    // TODO: Mache dies sicher...!
                    strcpy(csEntry.lineFileName, Line.FileName);
                }
                else
                {
                    // TODO: error report
                    //this->OnDbgHelpErr("SymGetLineFromAddr64",
                    //GetLastError(), s.AddrPC.Offset);
                }
            } // yes, we have SymGetLineFromAddr64()

            // show module info (SymGetModuleInfo64())
            if (CMUTIL_CALL(walker, GetModuleInfo, s.AddrPC.Offset,
                &Module) != FALSE)
            { // got module info OK
                // TODO: Mache dies sicher...!
                strcpy(csEntry.moduleName, Module.ModuleName);
                csEntry.baseOfImage = Module.BaseOfImage;
                strcpy(csEntry.loadedImageName, Module.LoadedImageName);
            } // got module info OK
            else
            {
                // TODO: error report
                //this->OnDbgHelpErr("SymGetModuleInfo64", GetLastError(),
                //s.AddrPC.Offset);
            }
        } // we seem to have a valid PC

        CallstackEntryType et = nextEntry;
        if (frameNum == 0)
            et = firstEntry;
        {
            CHAR buffer[STACKWALK_MAX_NAMELEN];
            if ((et != lastEntry) && (csEntry.offset != 0))
            {
                int len;
                if (csEntry.name[0] == 0)
                    strcpy(csEntry.name, "(function-name not available)");
                if (csEntry.undName[0] != 0)
                    strcpy(csEntry.name, csEntry.undName);
                if (csEntry.undFullName[0] != 0)
                    strcpy(csEntry.name, csEntry.undFullName);
                if (csEntry.lineFileName[0] == 0)
                {
                    strcpy(csEntry.lineFileName, "(filename not available)");
                    if (csEntry.moduleName[0] == 0)
                        strcpy(csEntry.moduleName,
                            "(module-name not available)");
                    len = _snprintf(buffer, STACKWALK_MAX_NAMELEN,
                        "%p (%s): %s: %s\n", (LPVOID)csEntry.offset,
                        csEntry.moduleName, csEntry.lineFileName, csEntry.name);
                }
                else
                    len = _snprintf(buffer, STACKWALK_MAX_NAMELEN,
                        "%s (%d): %s\n", csEntry.lineFileName,
                        csEntry.lineNumber, csEntry.name);
                readMemoryFunction(buffer, len, pUserData);
            }
        }

        if (s.AddrReturn.Offset == 0)
        {
            //this->OnCallstackEntry(lastEntry, csEntry);
            SetLastError(ERROR_SUCCESS);
            break;
        }
    } // for ( frameNum )

cleanup:
    if (pSym) walker->memst->Free(pSym);

    return TRUE;
}

struct WalkerTemp {
    int depth;
    int stdepth;
    CMUTIL_StringArray *res;
    CMUTIL_StackWalker_Internal *walker;
};

CMUTIL_STATIC void CMUTIL_StackWalkerGetStackIterator(
    const char *sinfo, int len, void *data)
{
    struct WalkerTemp *wtemp = (struct WalkerTemp*)data;
    if (wtemp->depth++ > wtemp->stdepth) {
        CMUTIL_String *item = CMUTIL_StringCreateInternal(
                    wtemp->walker->memst, len, NULL);
        CMUTIL_CALL(item, AddNString, sinfo, len);
        CMUTIL_CALL(wtemp->res, Add, item);
    }
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_StackWalkerGetStack(
    CMUTIL_StackWalker *walker, int skipdepth)
{
    CMUTIL_StackWalker_Internal *iwalker = (CMUTIL_StackWalker_Internal*)walker;
    CMUTIL_StringArray *res = CMUTIL_StringArrayCreateInternal(
                iwalker->memst, 10);
    struct WalkerTemp wtemp = { 0, skipdepth+1, res, iwalker };
    CMUTIL_StackWalkerShowCallstack(
        iwalker, CMUTIL_StackWalkerGetStackIterator, &wtemp);
    return res;
}

struct WalkerPrintTemp {
    int depth;
    int stdepth;
    CMUTIL_String *res;
};

CMUTIL_STATIC void CMUTIL_StackWalkerPrintStackIterator(
    const char *sinfo, int len, void *data)
{
    struct WalkerPrintTemp *wtemp = (struct WalkerPrintTemp*)data;
    if (wtemp->depth++ > wtemp->stdepth) {
        CMUTIL_CALL(wtemp->res, AddString, "\r\n");
        CMUTIL_CALL(wtemp->res, AddNString, sinfo, len);
    }
}

CMUTIL_STATIC void CMUTIL_StackWalkerPrintStack(
    CMUTIL_StackWalker *walker, CMUTIL_String *outbuf, int skipdepth)
{
    CMUTIL_StackWalker_Internal *iwalker = (CMUTIL_StackWalker_Internal*)walker;
    struct WalkerPrintTemp wtemp = { 0, skipdepth + 1, outbuf };
    CMUTIL_StackWalkerShowCallstack(
        iwalker, CMUTIL_StackWalkerPrintStackIterator, &wtemp);
}

CMUTIL_STATIC void CMUTIL_StackWalkerDestroy(
    CMUTIL_StackWalker *walker)
{
    CMUTIL_StackWalker_Internal *iwalker = (CMUTIL_StackWalker_Internal*)walker;
    iwalker->memst->Free(iwalker);
}

static CMUTIL_StackWalker_Internal g_cmutil_stackwalker = {
    {
        CMUTIL_StackWalkerGetStack,
        CMUTIL_StackWalkerPrintStack,
        CMUTIL_StackWalkerDestroy
    },
    CMUTIL_StackWalkerLoadModule,
    CMUTIL_StackWalkerGetModuleListTH32,
    CMUTIL_StackWalkerGetModuleListPSAPI,
    CMUTIL_StackWalkerLoadModules,
    CMUTIL_StackWalkerGetModuleInfo,
    NULL
};

CMUTIL_StackWalker *CMUTIL_StackWalkerCreateInternal(CMUTIL_Mem_st *memst)
{
    CMUTIL_StackWalker_Internal *res =
            memst->Alloc(sizeof(CMUTIL_StackWalker_Internal));
    memcpy(res, &g_cmutil_stackwalker, sizeof(CMUTIL_StackWalker_Internal));
    res->memst = memst;
    return (CMUTIL_StackWalker*)res;
}

CMUTIL_StackWalker *CMUTIL_StackWalkerCreate()
{
    return CMUTIL_StackWalkerCreateInternal(__CMUTIL_Mem);
}

#else

#include <execinfo.h>

typedef struct CMUTIL_StackWalker_Internal {
    CMUTIL_StackWalker	base;
    CMUTIL_Mem_st		*memst;
} CMUTIL_StackWalker_Internal;

void CMUTIL_CallStackInit()
{
    // do nothing
}

void CMUTIL_CallStackClear()
{
    // do nothing
}

#if defined(SUNOS)
typedef struct CMUTIL_StackWalkerCtx {
    int currdepth;
    int stdepth;
    CMUTIL_StringArray *stack;
    CMUTIL_String *buffer;
    CMUTIL_StackWalker_Internal *walker;
} CMUTIL_StackWalkerCtx;

CMUTIL_STATIC int CMUTIL_StackWalkerIterator(
    unsigned long pc, int sig, void* uarg)
{
    Dl_info dlip;

    if (dladdr((void*)pc, &dlip)) {
        CMUTIL_StackWalkerCtx *stk = (CMUTIL_StackWalkerCtx*)uarg;
        if (wtemp->depth++ > wtemp->stdepth) {
            if (wtemp->buffer) {
                CMUTIL_CALL(wtemp->buffer, AddPrint, "\n%08lx %s %s",
                    pc, dlip.dli_fname, dlip.dli_sname);
            } else {
                CMUTIL_String *item = CMUTIL_StringCreateInternal(
                            50, NULL, stk->walker->memst);
                CMUTIL_CALL(item, AddPrint, "%08lx %s %s",
                    pc, dlip.dli_fname, dlip.dli_sname);
                CMUTIL_CALL(item->stack, Add, item);
            }
            return 0;
        } else {
            return -1;
        }
    }
    else {
        return -1;
    }
}

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_StackWalkerGetStack(
    CMUTIL_StackWalker_Internal *walker, int skipdepth)
{
    ucontext_t ucp;
    if(getcontext(&ucp) == 0) {
        CMUTIL_StackWalkerCtx wtemp = {
            0, skipdepth+1, CMUTIL_StringArrayCreateInternal(walker->memst, 10),
            NULL, walker
        };
        walkcontext(&ucp, CMUTIL_StackWalkerIterator, &wtemp);
        return wtemp.stack;
    }
    return NULL;
}

CMUTIL_STATIC void CMUTIL_StackWalkerPrintStack(
    CMUTIL_StackWalker_Internal *walker, CMUTIL_String *outbuf, int skipdepth)
{
    ucontext_t ucp;
    if(getcontext(&ucp) == 0) {
        CMUTIL_StackWalkerCtx wtemp = {
            0, skipdepth + 1, NULL, outbuf, walker
        };
        walkcontext(&ucp, CMUTIL_StackWalkerIterator, &wtemp);
    }
}

#else	/* not SUNOS */

#define CMUTIL_STACK_MAX	1024

CMUTIL_STATIC CMUTIL_StringArray *CMUTIL_StackWalkerGetStack(
    CMUTIL_StackWalker *walker, int skipdepth)
{
    CMUTIL_StackWalker_Internal *iwalker = (CMUTIL_StackWalker_Internal*)walker;
    char **stacks = NULL;
    void *buffer[CMUTIL_STACK_MAX];
    int i, nptrs = backtrace(buffer, CMUTIL_STACK_MAX);
    if (nptrs > 0) {
        CMUTIL_StringArray *res = CMUTIL_StringArrayCreateInternal(
                    iwalker->memst, 10);
        if (nptrs > CMUTIL_STACK_MAX)
            nptrs = CMUTIL_STACK_MAX;
        stacks = backtrace_symbols(buffer, nptrs);
        if (stacks) {
            for (i=skipdepth+1; i<nptrs; i++)
                CMUTIL_CALL(res, Add, CMUTIL_StringCreateInternal(
                                iwalker->memst, 50, stacks[i]));
            free(stacks);
        }
        return res;
    }
    CMUTIL_UNUSED(walker);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_StackWalkerPrintStack(
    CMUTIL_StackWalker *walker, CMUTIL_String *outbuf, int skipdepth)
{
    char **stacks = NULL;
    void *buffer[CMUTIL_STACK_MAX];
    int i, nptrs = backtrace(buffer, CMUTIL_STACK_MAX);
    if (nptrs > 0) {
        if (nptrs > CMUTIL_STACK_MAX)
            nptrs = CMUTIL_STACK_MAX;
        stacks = backtrace_symbols(buffer, nptrs);
        if (stacks) {
            for (i=skipdepth+1; i<nptrs; i++)
                CMUTIL_CALL(outbuf, AddPrint, S_CRLF"%s", stacks[i]);
            free(stacks);
        }
    }
    CMUTIL_UNUSED(walker);
}

#endif


CMUTIL_STATIC void CMUTIL_StackWalkerDestroy(
    CMUTIL_StackWalker *walker)
{
    CMUTIL_StackWalker_Internal *iwalker = (CMUTIL_StackWalker_Internal*)walker;
    if (iwalker)
        iwalker->memst->Free(iwalker);
}

static CMUTIL_StackWalker_Internal g_cmutil_stackwalker = {
    {
        CMUTIL_StackWalkerGetStack,
        CMUTIL_StackWalkerPrintStack,
        CMUTIL_StackWalkerDestroy
    },
    NULL
};

CMUTIL_StackWalker *CMUTIL_StackWalkerCreateInternal(CMUTIL_Mem_st *memst)
{
    CMUTIL_StackWalker_Internal *res =
            memst->Alloc(sizeof(CMUTIL_StackWalker_Internal));
    memcpy(res, &g_cmutil_stackwalker, sizeof(CMUTIL_StackWalker_Internal));
    res->memst = memst;
    return (CMUTIL_StackWalker*)res;
}

CMUTIL_StackWalker *CMUTIL_StackWalkerCreate()
{
    return CMUTIL_StackWalkerCreateInternal(__CMUTIL_Mem);
}

#endif
