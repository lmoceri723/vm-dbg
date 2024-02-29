/* A debugger extension to dump information about PTE and PFN structures in my memory manager
 * Made by Landon Moceri on 2-26-2024 */

// cl /LD /Fo./build/ /Fe./build/ /Fd./build/ extension.c /link /def:plusextension.def /out:build/extension.dll
// C:/Users/ltm14/CLionProjects/dbg-ext/build/extension.dll

#include <Windows.h>
#include <dbgeng.h>

#include "../vm/include/structs.h"

#pragma comment(lib, "dbgeng.lib")

// The API version this extension is compatible with
#define EXT_MAJOR_VER 1
#define EXT_MINOR_VER 0

// Manually define IID_IDebugControl if it's not defined
#ifndef IID_IDebugControl
static const GUID IID_IDebugControl = { 0x5182e668, 0x105e, 0x416e, { 0xad, 0x92, 0x24, 0xef, 0x80, 0x04, 0x24, 0xba } };
#endif

// Manually define IID_IDebugDataSpaces if it's not defined
#ifndef IID_IDebugDataSpaces
static const GUID IID_IDebugDataSpaces = { 0x7a5e852f, 0x96e9, 0x468f, { 0xac, 0x57, 0x49, 0x2f, 0x33, 0x33, 0x84, 0xf3 } };
#endif


// Global variables
PDEBUG_CLIENT4 g_Client;
PDEBUG_CONTROL g_Control;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

// Extension API version
HRESULT CALLBACK DebugExtensionNotify(ULONG Notify, ULONG64 Argument)
{
    // We don't need to handle any notifications in this example
    return S_OK;
}

// Test command
HRESULT CALLBACK test(IDebugClient* Client, PCSTR args)
{
    IDebugControl* Control;
    if (Client->lpVtbl->QueryInterface(Client, &IID_IDebugControl, (void**)&Control) != S_OK)
    {
        return E_FAIL;
    }

    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Hello, world!\n");

    Control->lpVtbl->Release(Control);

    return S_OK;
}


HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags)
{
    *Version = ((EXT_MAJOR_VER << 16) | EXT_MINOR_VER);
    *Flags = 0;
    return S_OK;
}

HRESULT CALLBACK dpte(IDebugClient* Client, PCSTR args)
{
    // Initialize interfaces
    IDebugControl* Control;
    IDebugDataSpaces* DataSpaces;

    if (Client->lpVtbl->QueryInterface(Client, &IID_IDebugControl, (void**)&Control) != S_OK)
    {
        return E_FAIL;
    }
    HRESULT hr = Client->lpVtbl->QueryInterface(Client, &IID_IDebugDataSpaces, (void**)&DataSpaces);
    if (hr != S_OK)
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Failed to get DataSpaces, error: 0x%x\n", hr);
        Control->lpVtbl->Release(Control);
        return E_FAIL;
    }

    // Parse the PTE address from the arguments
    ULONG64 pte_address;
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "args: %s\n", args);
    sscanf(args, "%I64x", &pte_address);
    // Print the PTE address from the arguments
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE address: %I64x\n", pte_address);

    // Read the PTE from memory
    PTE pte;
    hr = DataSpaces->lpVtbl->ReadVirtualUncached(DataSpaces, pte_address, &pte, sizeof(PTE), NULL);
    if (hr != S_OK)
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "ReadVirtualUncached failed with error: 0x%x\n", hr);
        Control->lpVtbl->Release(Control);
        DataSpaces->lpVtbl->Release(DataSpaces);
        return hr;
    }

    // Print the PTE contents
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %I64x:\n", pte_address);
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Valid: %llu\n", pte.memory_format.valid);
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Frame number: %llu\n", pte.memory_format.frame_number);
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Age: %llu\n", pte.memory_format.age);

    return S_OK;
}

/*
// Command to dump a specific PTE
HRESULT CALLBACK dump_pte(IDebugClient* Client, PCSTR args)
{
    // Initialize interfaces
    IDebugControl2* Control;
    IDebugDataSpaces* DataSpaces;
    if (Client->lpVtbl->QueryInterface(Client, &IID_IDebugControl, (void**)&Control) != S_OK)
    {
        return E_FAIL;
    }
    if (Client->lpVtbl->QueryInterface(Client, &IID_IDebugDataSpaces, (void**)&DataSpaces) != S_OK)
    {
        Control->lpVtbl->Release(Control);
        return E_FAIL;
    }

    // Parse the PTE address from the arguments
    ULONG64 pte_address = strtoull(args, NULL, 16);

    // Retrieve the PTE from memory
    PTE pte;
    DataSpaces->lpVtbl->ReadVirtual(DataSpaces, pte_address, &pte, sizeof(PTE), NULL);

    BOOLEAN on_disc = FALSE;

    // Print the format of the PTE
    if (pte.memory_format.valid)
    {
        // Print correctly here

        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %llx is in memory format\n", pte_address);

    }
    else if (pte.disc_format.on_disc)
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %llx is in disc format\n", pte_address);
        on_disc = TRUE;
    }
    else if (pte.transition_format.frame_number != 0)
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %llx is in transition format\n", pte_address);
    }
    else
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %llx is uninitialized\n", pte_address);
    }

    // Print the PTE contents
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "PTE at %llx:\n", pte_address);
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Valid: %llu\n", pte.memory_format.valid);
    if (on_disc)
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Disc index: %llu\n", pte.disc_format.disc_index);
    }
    else
    {
        Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Frame number: %llu\n", pte.memory_format.frame_number);
    }
    Control->lpVtbl->Output(Control, DEBUG_OUTPUT_NORMAL, "Age: %llu\n", pte.memory_format.age);

    // Clean up
    DataSpaces->lpVtbl->Release(DataSpaces);
    Control->lpVtbl->Release(Control);

    return S_OK;
}
 */

HRESULT CALLBACK DebugExtensionUninitialize(void)
{
    // We don't need to do anything in this example
    return S_OK;
}

