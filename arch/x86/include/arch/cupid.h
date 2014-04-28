/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_CUPID_H
#define INCLUDE_ARCH_CUPID_H

enum cpuid_vendors {
    CPUID_VENDOR_OLDAMD = 0,
    CPUID_VENDOR_AMD,
    CPUID_VENDOR_INTEL,
    CPUID_VENDOR_VIA,
    CPUID_VENDOR_OLDTRANSMETA,
    CPUID_VENDOR_TRANSMETA,
    CPUID_VENDOR_CYRIX,
    CPUID_VENDOR_CENTAUR,
    CPUID_VENDOR_NEXGEN,
    CPUID_VENDOR_UMC,
    CPUID_VENDOR_SIS,
    CPUID_VENDOR_NSC,
    CPUID_VENDOR_RISE,
    CPUID_VENDOR_LAST
};

const char *cpuid_vendor_ids[CPUID_VENDOR_LAST] = {
    [CPUID_VENDOR_OLDAMD]       = "AMDisbetter!",
    [CPUID_VENDOR_AMD]          = "AuthenticAMD",
    [CPUID_VENDOR_INTEL]        = "GenuineIntel",
    [CPUID_VENDOR_VIA]          = "CentaurHauls",
    [CPUID_VENDOR_OLDTRANSMETA] = "TransmetaCPU",
    [CPUID_VENDOR_TRANSMETA]    = "GenuineTMx86",
    [CPUID_VENDOR_CYRIX]        = "CyrixInstead",
    [CPUID_VENDOR_CENTAUR]      = "CentaurHauls",
    [CPUID_VENDOR_NEXGEN]       = "NexGenDriven",
    [CPUID_VENDOR_UMC]          = "UMC UMC UMC ",
    [CPUID_VENDOR_SIS]          = "SIS SIS SIS ",
    [CPUID_VENDOR_NSC]          = "Gende by NSC",
    [CPUID_VENDOR_RISE]         = "RiseRiseRise",
};

enum cpuid_feature_flags {
    CPUID_FEAT_ECX_SSE3         = 1 << 0, 
    CPUID_FEAT_ECX_PCLMUL       = 1 << 1,
    CPUID_FEAT_ECX_DTES64       = 1 << 2,
    CPUID_FEAT_ECX_MONITOR      = 1 << 3,  
    CPUID_FEAT_ECX_DS_CPL       = 1 << 4,  
    CPUID_FEAT_ECX_VMX          = 1 << 5,  
    CPUID_FEAT_ECX_SMX          = 1 << 6,  
    CPUID_FEAT_ECX_EST          = 1 << 7,  
    CPUID_FEAT_ECX_TM2          = 1 << 8,  
    CPUID_FEAT_ECX_SSSE3        = 1 << 9,  
    CPUID_FEAT_ECX_CID          = 1 << 10,
    CPUID_FEAT_ECX_FMA          = 1 << 12,
    CPUID_FEAT_ECX_CX16         = 1 << 13, 
    CPUID_FEAT_ECX_ETPRD        = 1 << 14, 
    CPUID_FEAT_ECX_PDCM         = 1 << 15, 
    CPUID_FEAT_ECX_DCA          = 1 << 18, 
    CPUID_FEAT_ECX_SSE4_1       = 1 << 19, 
    CPUID_FEAT_ECX_SSE4_2       = 1 << 20, 
    CPUID_FEAT_ECX_x2APIC       = 1 << 21, 
    CPUID_FEAT_ECX_MOVBE        = 1 << 22, 
    CPUID_FEAT_ECX_POPCNT       = 1 << 23, 
    CPUID_FEAT_ECX_AES          = 1 << 25, 
    CPUID_FEAT_ECX_XSAVE        = 1 << 26, 
    CPUID_FEAT_ECX_OSXSAVE      = 1 << 27, 
    CPUID_FEAT_ECX_AVX          = 1 << 28,
 
    CPUID_FEAT_EDX_FPU          = 1 << 0,  
    CPUID_FEAT_EDX_VME          = 1 << 1,  
    CPUID_FEAT_EDX_DE           = 1 << 2,  
    CPUID_FEAT_EDX_PSE          = 1 << 3,  
    CPUID_FEAT_EDX_TSC          = 1 << 4,  
    CPUID_FEAT_EDX_MSR          = 1 << 5,  
    CPUID_FEAT_EDX_PAE          = 1 << 6,  
    CPUID_FEAT_EDX_MCE          = 1 << 7,  
    CPUID_FEAT_EDX_CX8          = 1 << 8,  
    CPUID_FEAT_EDX_APIC         = 1 << 9,  
    CPUID_FEAT_EDX_SEP          = 1 << 11, 
    CPUID_FEAT_EDX_MTRR         = 1 << 12, 
    CPUID_FEAT_EDX_PGE          = 1 << 13, 
    CPUID_FEAT_EDX_MCA          = 1 << 14, 
    CPUID_FEAT_EDX_CMOV         = 1 << 15, 
    CPUID_FEAT_EDX_PAT          = 1 << 16, 
    CPUID_FEAT_EDX_PSE36        = 1 << 17, 
    CPUID_FEAT_EDX_PSN          = 1 << 18, 
    CPUID_FEAT_EDX_CLF          = 1 << 19, 
    CPUID_FEAT_EDX_DTES         = 1 << 21, 
    CPUID_FEAT_EDX_ACPI         = 1 << 22, 
    CPUID_FEAT_EDX_MMX          = 1 << 23, 
    CPUID_FEAT_EDX_FXSR         = 1 << 24, 
    CPUID_FEAT_EDX_SSE          = 1 << 25, 
    CPUID_FEAT_EDX_SSE2         = 1 << 26, 
    CPUID_FEAT_EDX_SS           = 1 << 27, 
    CPUID_FEAT_EDX_HTT          = 1 << 28, 
    CPUID_FEAT_EDX_TM1          = 1 << 29, 
    CPUID_FEAT_EDX_IA64         = 1 << 30,
    CPUID_FEAT_EDX_PBE          = 1 << 31
};

static __always_inline void cupid_id(char *cpuid)
{
    asm volatile("cpuid":"=a" (*cpuid), "=b" (*(cpuid + 1)), "=c" (*(cpuid + 2)), "=d" (*(cpuid + 3))
                 : "a" (0));
}

static __always_inline void cpuid_feature_flags(int *ecx_flags, int *edx_flags)
{
    asm volatile("cpuid": "=c" (*ecx_flags), "=d" (*edx_flags)
                 : "a" (1));
}

#endif
