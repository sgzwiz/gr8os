//
// <kd.h> built by header file parser at 09:59:08  17 Apr 2008
// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)
//

#pragma once

KESYSAPI
VOID
KEAPI
KiDebugPrintRaw(
	PCHAR String
	);

KESYSAPI
VOID
KECDECL
KiDebugPrint(
	PCHAR FormatString,
	...
	);

