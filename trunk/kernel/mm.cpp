//
// FILE:		mm.cpp
// CREATED:		17-Feb-2008  by Great
// PART:        MM
// ABSTRACT:
//			Memory Management routines
//

#include "common.h"

MUTEX MmPageDatabaseLock;

VOID
KEAPI
MiMapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to kernel virtual address space
--*/
{
	VirtualAddress = (PVOID) ALIGN_DOWN ((ULONG)VirtualAddress, PAGE_SIZE);
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	ExAcquireMutex (&MmPageDatabaseLock);

	for( ULONG i=0; i<PageCount; i++ )
	{
		MiWriteValidKernelPte (PointerPte);
		PointerPte->u1.e1.PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + i;

		KdPrint(("MM: Mapped %08x [<-%08x] pte %08x\n", (ULONG)VirtualAddress + (i<<PAGE_SHIFT), PointerPte->u1.e1.PageFrameNumber<<PAGE_SHIFT, PointerPte));

		MmInvalidateTlb (VirtualAddress);

		PointerPte = MiNextPte (PointerPte);
		*(ULONG*)&VirtualAddress += PAGE_SIZE;
	}

	ExReleaseMutex (&MmPageDatabaseLock);
}

PVOID
KEAPI
MmMapPhysicalPages(
	ULONG PhysicalAddress,
	ULONG PageCount
	)
/*++
	Map physical pages to an arbitrary virtual address.
	Returns the virtual address of the mapped pages
--*/
{
	ExAcquireMutex (&MmPageDatabaseLock);

	ULONG StartVirtual = MM_CRITICAL_AREA;
	PMMPTE PointerPte = MiGetPteAddress (StartVirtual);

	for (ULONG i=0; i<MM_CRITICAL_AREA_PAGES; i++)
	{
		if ( *(ULONG*)PointerPte == 0 )
		{
			BOOLEAN Free = 1;

			for ( ULONG j=i+1; j<i+PageCount; j++ )
			{
				Free &= ( *(ULONG*)&PointerPte[j] == 0 );
			}

			if (Free)
			{
				ULONG VirtualAddress = StartVirtual;

				for( ULONG j=i; j<PageCount; j++ )
				{
					MiWriteValidKernelPte (PointerPte);
					PointerPte->u1.e1.PageFrameNumber = (PhysicalAddress >> PAGE_SHIFT) + j;

					MmInvalidateTlb ((PVOID)VirtualAddress);

					PointerPte = MiNextPte (PointerPte);
					VirtualAddress += PAGE_SIZE;
				}

				ExReleaseMutex (&MmPageDatabaseLock);
				return (PVOID)StartVirtual;
			}

			i += PageCount - 1;
		}
	}

	ExReleaseMutex (&MmPageDatabaseLock);
	return NULL;
}


VOID
KEAPI
MiUnmapPhysicalPages(
	PVOID VirtualAddress,
	ULONG PageCount
	)
/*++
	Unmap physical pages from the kernel virtual address space
--*/
{
	VirtualAddress = (PVOID) ALIGN_DOWN ((ULONG)VirtualAddress, PAGE_SIZE);
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	//KeZeroMemory( PointerPte, PageCount*sizeof(MMPTE) );
	for (ULONG i=0; i<PageCount; i++)
	{
		KdPrint(("MM: Unmapping %08x [->%08x] pte %08x\n", (ULONG)VirtualAddress + (i<<PAGE_SHIFT), (PointerPte->u1.e1.PageFrameNumber)<<PAGE_SHIFT, PointerPte));
		
		PointerPte->RawValue = 0;

		MmInvalidateTlb ((PVOID)((ULONG)VirtualAddress + (i<<PAGE_SHIFT)));

		PointerPte = MiNextPte (PointerPte);
	}
}

char MmDebugBuffer[1024];

VOID
KEAPI
MmAccessFault(
	PVOID VirtualAddress,
	PVOID FaultingAddress
	)
/*++
	Resolve access fault
--*/
{
	sprintf( MmDebugBuffer, "MM: Page fault at %08x, referring code: %08x\n", VirtualAddress, FaultingAddress );
	KiDebugPrintRaw( MmDebugBuffer );

	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [invalid page]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_PAGEDOUT:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [paging not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_TRIMMED:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [trimming not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}

	case PTE_TYPE_VIEW:
		{
			sprintf( MmDebugBuffer, "MM: Access violation in %08x [views not supported]\n", VirtualAddress );
			KiDebugPrintRaw( MmDebugBuffer );

			__asm lea esi, [MmDebugBuffer]
			__asm xor ax, ax
			__asm int 0x30

			break;
		}
	}

	KeBugCheck (KERNEL_MODE_EXCEPTION_NOT_HANDLED,
				STATUS_ACCESS_VIOLATION,
				(ULONG)VirtualAddress,
				0,
				0
				);
	
	KiStopExecution( );
}


VOID
KEAPI
MmCreateAddressSpace(
	PPROCESS Process
	)
/*++
	Creates process' address space
--*/
{
	KiDebugPrintRaw( "MM: MmCreateAddressSpace unimplemented\n" );

	KeBugCheck (MEMORY_MANAGEMENT,
				__LINE__,
				0,
				0,
				0);
}


KESYSAPI
BOOLEAN
KEAPI
MmIsAddressValid(
	IN PVOID VirtualAddress
	)
/*++
	This function checks that specified virtual address is valid
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);
	return PointerPte->u1.e1.Valid;
}

KESYSAPI
ULONG
KEAPI
MmIsAddressValidEx(
	IN PVOID VirtualAddress
	)
/*++
	This function checks that specified virtual address is valid and determines the page type
--*/
{
	PMMPTE PointerPte = MiGetPteAddress (VirtualAddress);

	if (PointerPte->u1.e1.Valid)
	{
		return PageStatusNormal;
	}

	switch (PointerPte->u1.e1.PteType)
	{
	case PTE_TYPE_NORMAL_OR_NOTMAPPED:
		return PageStatusFree;

	case PTE_TYPE_TRIMMED:
		return PageStatusTrimmed;
	
	case PTE_TYPE_PAGEDOUT:
		return PageStatusPagedOut;

	case PTE_TYPE_VIEW:
		return PageStatusView;
	}

	KeBugCheck (MEMORY_MANAGEMENT,
				__LINE__,
				(ULONG)PointerPte,
				PointerPte->u1.e1.PteType,
				0);
}


VOID
KEAPI
MiZeroPageThread(
	)
{
	for(;;)
	{
	}
}


VOID
KEAPI
MmInitSystem(
	)
{
	ExInitializeMutex (&MmPageDatabaseLock);
}