/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * reboot.c
 *
 * Copyright 2019, 2024 Phoenix Systems
 * Author: Jan Sikorski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/reboot.h>
#include <sys/platform.h>
#if defined(__CPU_ZYNQMP)
#include <phoenix/arch/aarch64/zynqmp/zynqmp.h>
#elif defined(__CPU_GENERIC)
#include <phoenix/arch/aarch64/generic/generic.h>
#else
#error "Unsupported TARGET"
#endif


int reboot(int magic)
{
	platformctl_t pctl = { 0 };

	pctl.action = pctl_set;
	pctl.type = pctl_reboot;
#if defined(__CPU_ZYNQMP)
	pctl.reboot.magic = magic;
#else
	pctl.task.reboot.magic = magic;
#endif

	return platformctl(&pctl);
}


int reboot_reason(uint32_t *val)
{
	platformctl_t pctl = { 0 };

	pctl.action = pctl_get;
	pctl.type = pctl_reboot;

	*val = 0;
	if (platformctl(&pctl) < 0) {
		return -1;
	}

#if defined(__CPU_ZYNQMP)
	*val = pctl.reboot.reason;
#else
	*val = pctl.task.reboot.reason;
#endif
	return 0;
}
