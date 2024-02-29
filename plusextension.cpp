#include <cstdio>
#include <windows.h>
#include <Dbgeng.h>
#include <string>
#include "../vm/include/structs.h"
#include "../vm/include/system.h"
#include "../vm/include/userapp.h"

// Compile command
// cl /EHsc /LD plusextension.cpp /link /def:plusextension.def

// Load command
// .load C:\Users\ltm14\CLionProjects/dbg-ext/plusextension.dll

PVOID va_base;
PPTE pte_base;
ULONG64 va_base_address;
ULONG64 pte_base_address;

PPTE pte_from_va(PVOID virtual_address);
PVOID pte_to_va(ULONG64 pte_address);

#pragma comment(lib, "dbgeng.lib")

extern "C" HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags) {
    *Version = DEBUG_EXTENSION_VERSION(1, 0);
    *Flags = 0;

    return S_OK;
}

extern "C" HRESULT CALLBACK vars_load(PDEBUG_CLIENT Client, PCSTR args) {
    // Get the IDebugClient interface
    IDebugClient* client;
    DebugCreate(__uuidof(IDebugClient), (void**)&client);

    IDebugControl* Control;
    client->QueryInterface(__uuidof(IDebugControl), (void**)&Control);

    // Get the IDebugSymbols interface
    IDebugSymbols* symbols;
    client->QueryInterface(__uuidof(IDebugSymbols), (void**)&symbols);

    // Get the IDebugDataSpaces interface
    IDebugDataSpaces* dataSpaces;
    client->QueryInterface(__uuidof(IDebugDataSpaces), (void**)&dataSpaces);

    // Get the addresses of va_base and pte_base
    symbols->GetOffsetByName("va_base", &va_base_address);
    symbols->GetOffsetByName("pte_base", &pte_base_address);

    // Read the values of va_base and pte_base

    dataSpaces->ReadVirtual(va_base_address, &va_base, sizeof(PVOID), NULL);
    dataSpaces->ReadVirtual(pte_base_address, &pte_base, sizeof(PPTE), NULL);

    Control->Output(DEBUG_OUTPUT_NORMAL, "va_base: 0x%p\n", va_base);
    Control->Output(DEBUG_OUTPUT_NORMAL, "pte_base: 0x%p\n", pte_base);

    // Release the interfaces
    dataSpaces->Release();
    symbols->Release();
    Control->Release();
    client->Release();

    return S_OK;
}

extern "C" void CALLBACK DebugExtensionNotify(ULONG Notify, ULONG64 Argument) {
    // We don't need to handle any notifications in this example
}

extern "C" void CALLBACK DebugExtensionUninitialize() {
    // We don't need to do anything in this example
}

std::string format(std::string address) {
    // Remove '0x' from the front
    if (address.substr(0, 2) == "0x") {
        address.erase(0, 2);
    }

    // Remove '`' from the middle
    size_t pos = address.find('`');
    if (pos != std::string::npos) {
        address.erase(pos, 1);
    }

    return address;
}

extern "C" HRESULT CALLBACK test(PDEBUG_CLIENT Client, PCSTR args) {
    PDEBUG_CONTROL Control;
    if (Client->QueryInterface(__uuidof(IDebugControl), (void**)&Control) != S_OK) {
        return E_FAIL;
    }

    Control->Output(DEBUG_OUTPUT_NORMAL, "Hello, world!\n");
    Control->Release();

    return S_OK;
}

extern "C" HRESULT CALLBACK pte(PDEBUG_CLIENT Client, PCSTR args) {
    IDebugControl* Control;
    IDebugDataSpaces* DataSpaces;
    HRESULT hr;
    ULONG64 pte_address;
    PTE pte;
    PVOID va;

    // Format the arguments to a format that can be read by sscanf
    std::string argsStr(args);
    std::string formatted = format(argsStr);

    if (Client->QueryInterface(__uuidof(IDebugControl), (void**)&Control) != S_OK)
    {
        return E_FAIL;
    }

    hr = Client->QueryInterface(__uuidof(IDebugDataSpaces), (void**)&DataSpaces);
    if (hr != S_OK)
    {
        Control->Output(DEBUG_OUTPUT_NORMAL, "Failed to get DataSpaces, error: 0x%x\n", hr);
        Control->Release();
        return E_FAIL;
    }

    // Parse the PTE address from the arguments
    sscanf(formatted.c_str(), "%I64x", &pte_address);
    // Print the PTE address from the arguments

    // Read the PTE from memory
    hr = DataSpaces->ReadVirtual(pte_address, &pte, sizeof(PTE), NULL);
    if (hr != S_OK)
    {
        Control->Output(DEBUG_OUTPUT_NORMAL, "Failed to read PTE, error: 0x%x\n", hr);
        Control->Release();
        DataSpaces->Release();
        return E_FAIL;
    }

    // Convert the PTE to a VA and print the VA
    va = pte_to_va(pte_address);
    Control->Output(DEBUG_OUTPUT_NORMAL, "VA: 0x%p\n", va);

    // Print the PTE contents
    if (pte.memory_format.valid) {
        Control->Output(DEBUG_OUTPUT_NORMAL, "PTE at %s is in memory format\n", args);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Valid: 0x%llx\n", pte.memory_format.valid);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Frame number: 0x%I64x\n", pte.memory_format.frame_number);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Age: 0x%llx\n", pte.memory_format.age);
    }
    else if (pte.disc_format.on_disc)
    {
        Control->Output(DEBUG_OUTPUT_NORMAL, "PTE at %s is in disc format\n", args);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Valid: 0x%llx\n", pte.disc_format.always_zero);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Disc index: 0x%I64x\n", pte.disc_format.disc_index);
        Control->Output(DEBUG_OUTPUT_NORMAL, "On disc: 0x%llx\n", pte.disc_format.on_disc);
    }
    else if (pte.transition_format.frame_number != 0)
    {
        Control->Output(DEBUG_OUTPUT_NORMAL, "PTE at %s is in transition format\n", args);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Valid: 0x%llx\n", pte.transition_format.always_zero);
        Control->Output(DEBUG_OUTPUT_NORMAL, "Frame number: 0x%I64x\n", pte.transition_format.frame_number);
        Control->Output(DEBUG_OUTPUT_NORMAL, "On disc: 0x%llx\n", pte.transition_format.always_zero2);
    }
    else
    {
        Control->Output(DEBUG_OUTPUT_NORMAL, "PTE at %s is uninitialized\n", args);
    }

    Control->Release();
    DataSpaces->Release();

    return S_OK;
}

// Write a command to convert a va to a pte
extern "C" HRESULT CALLBACK va(PDEBUG_CLIENT Client, PCSTR args) {
    IDebugControl *Control;
    IDebugDataSpaces *DataSpaces;

    std::string argsStr(args);
    std::string formatted = format(argsStr);

    if (Client->QueryInterface(__uuidof(IDebugControl),(void**)&Control) != S_OK) {
    return E_FAIL;
    }

    HRESULT hr = Client->QueryInterface(__uuidof(IDebugDataSpaces), (void **) &DataSpaces);
    if (hr != S_OK) {
    Control->Output(DEBUG_OUTPUT_NORMAL,"Failed to get DataSpaces, error: 0x%x\n", hr);
    Control->Release();

    return E_FAIL;
    }

    // Parse the VA address from the arguments
    ULONG64 va_address;
    sscanf(formatted.c_str(),"%I64x", &va_address);

    // Convert the VA to a PTE and print the PTE
    PPTE pte = pte_from_va((PVOID) va_address);
    Control->Output(DEBUG_OUTPUT_NORMAL,"PTE: 0x%I64x\n", pte);

    Control->Release();

    DataSpaces->Release();

    return S_OK;
}

ULONG64 va_to_pte(PVOID virtual_address)
{
    // We can compute the difference between the first va and our va
    // This will be equal to the difference between the first pte and the pte we want
    ULONG_PTR difference = (ULONG_PTR) virtual_address - (ULONG_PTR) va_base;
    difference /= 4096;
    ULONG64 base_address = reinterpret_cast<ULONG64>(pte_base);

    return base_address + difference;
}

PVOID pte_to_va(ULONG64 pte_address)
{
    ULONG64 base_address = reinterpret_cast<ULONG64>(pte_base);
    ULONG64 difference = pte_address - base_address;

    difference *= 4096;

    PVOID result = (PVOID) ((ULONG64) va_base + difference);

    return result;
}

//PPFN pfn_from_frame_number(ULONG64 frame_number)
//{
//    // Again, the compiler implicitly multiplies frame number by PFN size
//    return pfn_base + frame_number;
//}
//
//ULONG64 frame_number_from_pfn(PPFN pfn)
//{
//    return pfn - pfn_base;
//}
