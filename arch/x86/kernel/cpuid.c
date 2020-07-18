/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>

#include <arch/cpuid.h>

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

uint32_t cpuid_ecx, cpuid_edx;
char cpuid_id[10];

static void cpuid_get_id(char *cpuid)
{
    asm volatile("cpuid": "=a" (*cpuid), "=b" (*(cpuid + 1)), "=c" (*(cpuid + 2)), "=d" (*(cpuid + 3))
                 : "a" (0));
}

static void cpuid_get_feature_flags(uint32_t *ecx_flags, uint32_t *edx_flags)
{
    uint32_t eax = 1, ebx;
    asm volatile("cpuid": "=c" (*ecx_flags), "=d" (*edx_flags), "=b" (ebx), "+a" (eax));
}

void cpuid_init(void)
{
    cpuid_get_id(cpuid_id);
    cpuid_get_feature_flags(&cpuid_ecx, &cpuid_edx);
}

