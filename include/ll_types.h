/*
 * LavaLite - High-Throughput / HPC Scheduler
 *
 * Copyright (C) 2025, LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

// LavaLite unified type definitions
// All boolean logic uses XDR/TIRPC bool_t (32-bit integer)
// Lowercase true/false map directly to TRUE/FALSE for stylistic consistency.

#include <stdint.h>
#include <rpc/types.h> // defines: bool_t, TRUE, FALSE

// bool_t must come strictly from <rpc/types.h>. Do not redefine it.
// If the build system detects a missing or broken rpc/types.h,
// automake will fail — as intended.

// Enforce lowercase boolean literals
#ifndef true
#define true TRUE
#endif

#ifndef false
#define false FALSE
#endif

// LavaLite uses plain C types.
// No typedef onanism. True C style. A man style.

// Keep only protocol-specific typedefs if needed.
// (Currently none.)    // consistent port abstraction
