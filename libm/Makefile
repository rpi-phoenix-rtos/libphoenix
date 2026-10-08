#
# Makefile for math library
#
# Copyright 2025-2026 Phoenix Systems
# Author: Mikolaj Matalowski
#
# This file is part of Phoenix-RTOS.
#
# SPDX-License-Identifier: BSD-3-Clause
#

LIBM_USE_LIBMCS ?= n
LIBM_LIBMCS_DAZ ?= n
LIBM_WANT_COMPLEX ?= y
LIBM_USE_HW ?= y

CFLAGS += -Ilibm/include
CFLAGS += -I$(BUILD_DIR)/include

$(foreach v,LIBM_USE_LIBMCS LIBM_USE_HW LIBM_WANT_COMPLEX LIBM_LIBMCS_DAZ, $(if $(filter-out y n,$($(v))),$(error $(v) must be set to 'y' or 'n', got '$($(v))')))

# Assert on invalid configuration
ifeq ($(LIBM_USE_LIBMCS), n)
  ifeq ($(LIBM_LIBMCS_DAZ), y)
    $(error "Invalid libm configuration. FPU DAZ option is only valid with libmcs")
  endif
endif

# Add source files for hardware accelerated math
HW_OBJS :=
ifeq ($(LIBM_USE_HW), y)
  include libm/arch/Makefile
endif

SW_OBJS := 
# Build for libmcs
ifeq ($(LIBM_USE_LIBMCS), y)
  ifeq ($(LIBM_WANT_COMPLEX), y)
    SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/complexd/internal/*.c)))
    SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/complexd/*.c)))
    SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/complexf/internal/*.c)))
    SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/complexf/*.c)))
  endif

  # fenv.c implementation is target specific
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o, $(filter-out %fenv.c,$(wildcard libm/libmcs/libm/common/*.c))))
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/mathf/*.c)))
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/mathf/internal/*.c)))
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/mathd/*.c)))
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o,$(wildcard libm/libmcs/libm/mathd/internal/*.c)))

  CFLAGS += -DLIBM_LIBMCS_IMPL
else
  ifeq ($(LIBM_WANT_COMPLEX), y)
    SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o, libm/phoenix/complex.c))
  endif

  # Build for phoenix implementation
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o, $(patsubst %complex.c,, $(wildcard libm/phoenix/*.c))))

  # FreeBSD msun sources behind the transcendental functions (see msun/msun.h).
  # Their extra-precision steps (argument reduction in e_rem_pio2, the log/exp
  # splits in e_pow, ...) assume every product is rounded on its own, so they
  # are built without FMA contraction, which gcc enables by default.
  SW_OBJS += $(addprefix $(PREFIX_O), $(patsubst %.c,%.o, $(wildcard libm/phoenix/msun/*.c)))
  $(PREFIX_O)libm/phoenix/msun/%.o: CFLAGS += -ffp-contract=off

  # RPi4 fork: libm/phoenix keeps one file per family (exp.c, power.c, trig.c,
  # ...) on top of msun, not upstream's one-file-per-function mathd/ and mathf/
  # (upstream's split carries the old series implementations msun replaced).
  # Its sqrt/floor/ceil/trunc/round/fabs already use the instructions through
  # <arch.h>, and libm/arch/ would define them a second time: libm/arch/ serves
  # the libmcs build only.
  HW_OBJS :=

  CFLAGS += -Ilibm/phoenix
endif

# Clear functions that already implemented in hardware
# fixme: This currently clears long double versions from libmcs implementation for
# math functions with hardware implementation.
OBJS += $(foreach f,$(SW_OBJS),$(if $(filter $(notdir $(f)),$(notdir $(HW_OBJS))),,$(f)))
OBJS += $(HW_OBJS)
