//////////////////////////////////////////////////////////////////////////
//                                                                      //
//      NES Memory Management Unit                                      //
//                                                           Norix      //
//                                               written     2001/02/21 //
//                                               last modify ----/--/-- //
//////////////////////////////////////////////////////////////////////////

/*
MMU
메모리 관리 장치(Memory Management Unit, 줄여서 MMU)는 CPU가 메모리에 접근하는 것을 관리하는 컴퓨터 하드웨어 부품이다. 
가상 메모리 주소를 실제 메모리 주소로 변환하며, 메모리 보호, 캐시 관리, 버스 중재 등의 
역할을 담당하며 간단한 8비트 아키텍처에서는 뱅크 스위칭을 담당하기도 한다.
https://ko.wikipedia.org/wiki/%EB%B1%85%ED%81%AC_%EC%A0%84%ED%99%98 (뱅크 스위칭)
*/

#define	WIN32_LEAN_AND_MEAN		//WIN32_LEAN_AND_MEAN 매크로를 정의하여 빌드 시간을 단축할 수 있다.
#include <windows.h>
#include "typedef.h"
#include "macro.h"
#include "DebugOut.h"

#include "mmu.h"

// CPU 메모리 뱅크
LPBYTE	CPU_MEM_BANK[8];	// 8K단위
BYTE	CPU_MEM_TYPE[8];
INT	CPU_MEM_PAGE[8];	// 상태 저장용

// PPU 메모리 뱅크
LPBYTE	PPU_MEM_BANK[12];	// 1K단위
BYTE	PPU_MEM_TYPE[12];
INT	PPU_MEM_PAGE[12];	// 상태 저장용
BYTE	CRAM_USED[16];		// 상태 저장용

// NES메모리
BYTE	RAM [  8*1024];		// NES 내장RAM
BYTE	WRAM[128*1024];		// 워크/백업 RAM
BYTE	DRAM[ 40*1024];		// 디스크 시스템 RAM (dynamic random access memory)
BYTE	XRAM[  8*1024];		// 더미 뱅크

BYTE	CRAM[ 32*1024];		// 문자 패턴RAM
BYTE	VRAM[  4*1024];		// 이름/테이블 속성RAM

BYTE	SPRAM[0x100];		// 스프라이트RAM ( 스프라이트는 영상 속에 작은 2차원 영상이나 애니메이션을 합성하는 기술이다. )
BYTE	BGPAL[0x10];		// BG팔레트
BYTE	SPPAL[0x10];		// SP팔레트

// 레지스터
BYTE	CPUREG[0x18];		// Nes $4000-$4017 (CPU 레지스터 범위)  
BYTE	PPUREG[0x04];		// Nes $2000-$2003 (PPU 레지스터 범위)

// Frame-IRQ레지스터($4017)
BYTE	FrameIRQ;

// PPU내부 레지스터
BYTE	PPU56Toggle;		// $2005-$2006 Toggle
BYTE	PPU7_Temp;		// $2007 read buffer
WORD	loopy_t;		// same as $2005/$2006
WORD	loopy_v;		// same as $2005/$2006
WORD	loopy_x;		// tile x offset

// ROM데이터 포인터
LPBYTE	PROM;		// PROM ptr
LPBYTE	VROM;		// VROM ptr

// For dis...
LPBYTE	PROM_ACCESS = NULL;

// ROM 뱅크 크기
INT	PROM_8K_SIZE, PROM_16K_SIZE, PROM_32K_SIZE;
INT	VROM_1K_SIZE, VROM_2K_SIZE, VROM_4K_SIZE,  VROM_8K_SIZE;

//
// 전체 메모리 / 레지스터 등의 초기화
//
void	NesSub_MemoryInitial()
{
INT	i;

	// 메모리 클리어
	ZeroMemory( RAM,    8*1024 ); // ZeroMemory => c언어의 memset과 비슷
	ZeroMemory( WRAM, 128*1024 );
	ZeroMemory( DRAM,  40*1024 );
	ZeroMemory( CRAM,  32*1024 );
	ZeroMemory( VRAM,   4*1024 );

	ZeroMemory( SPRAM, 0x100 );
	ZeroMemory( BGPAL, 0x10 );
	ZeroMemory( SPPAL, 0x10 );

	ZeroMemory( CPUREG, 0x18 );
	ZeroMemory( PPUREG, 4 );

	FrameIRQ = 0xC0;		

	PROM = VROM = NULL;	// PROM , VROM 은 NULL로 초기화

	// 0 나누기 방지 ? 
	PROM_8K_SIZE = PROM_16K_SIZE = PROM_32K_SIZE = 1; 		// PROM SIZE 1로 초기화
	VROM_1K_SIZE = VROM_2K_SIZE = VROM_4K_SIZE = VROM_8K_SIZE = 1;	// VROM SIZE 1로 초기화

	// 디폴트 뱅크 설정
	for( i = 0; i < 8; i++ ) {
		CPU_MEM_BANK[i] = NULL;
		CPU_MEM_TYPE[i] = BANKTYPE_ROM;
		CPU_MEM_PAGE[i] = 0;
	}

	// 내장 RAM/WRAM
	SetPROM_Bank( 0, RAM,  BANKTYPE_RAM );	//PROM bank 설정부분
	SetPROM_Bank( 3, WRAM, BANKTYPE_RAM );	//SetPROM_Bank( BYTE page, LPBYTE ptr, BYTE type );

	// 더미
	SetPROM_Bank( 1, XRAM, BANKTYPE_ROM );	// XRAM은 더미 뱅크
	SetPROM_Bank( 2, XRAM, BANKTYPE_ROM );

	for( i = 0; i < 8; i++ ) {
		CRAM_USED[i] = 0;			// CRAM_USED[16]은 PPU 메모리 뱅크의 상태저장용
	}

	// PPU VROM 뱅크 설정
//	SetVRAM_Mirror( VRAM_MIRROR4 );
}

// ========= CPU ROM bank ===============
void	SetPROM_Bank( BYTE page, LPBYTE ptr, BYTE type )	// PROM(프로그램 가능 판독 전용 메모리)
{
	CPU_MEM_BANK[page] = ptr;
	CPU_MEM_TYPE[page] = type;
	CPU_MEM_PAGE[page] = 0;
}
//Set PROM 8K Bank
void	SetPROM_8K_Bank( BYTE page, INT bank )
{
	bank %= PROM_8K_SIZE;
	CPU_MEM_BANK[page] = PROM+0x2000*bank;
	CPU_MEM_TYPE[page] = BANKTYPE_ROM;
	CPU_MEM_PAGE[page] = bank;
}
//Set PROM 16K Bank
void	SetPROM_16K_Bank( BYTE page, INT bank )
{
	SetPROM_8K_Bank( page+0, bank*2+0 );
	SetPROM_8K_Bank( page+1, bank*2+1 );
}
//Set PROM 32k Bank
void	SetPROM_32K_Bank( INT bank )
{
	SetPROM_8K_Bank( 4, bank*4+0 );
	SetPROM_8K_Bank( 5, bank*4+1 );
	SetPROM_8K_Bank( 6, bank*4+2 );
	SetPROM_8K_Bank( 7, bank*4+3 );
}
//Set PROM 32K Bank
void	SetPROM_32K_Bank( INT bank0, INT bank1, INT bank2, INT bank3 )
{
	SetPROM_8K_Bank( 4, bank0 );
	SetPROM_8K_Bank( 5, bank1 );
	SetPROM_8K_Bank( 6, bank2 );
	SetPROM_8K_Bank( 7, bank3 );
}

// =========== PPU VROM bank ===========
// PPU(Picture Processing Unit,사진 처리 장치)
void	SetVROM_Bank( BYTE page, LPBYTE ptr, BYTE type )
{
	PPU_MEM_BANK[page] = ptr;
	PPU_MEM_TYPE[page] = type;
	PPU_MEM_PAGE[page] = 0;
}
//Set VROM 1K Bank
void	SetVROM_1K_Bank( BYTE page, INT bank )
{
	bank %= VROM_1K_SIZE;
	PPU_MEM_BANK[page] = VROM+0x0400*bank;
	PPU_MEM_TYPE[page] = BANKTYPE_VROM;
	PPU_MEM_PAGE[page] = bank;
}
//Set VROM 2K Bank
void	SetVROM_2K_Bank( BYTE page, INT bank )
{
	SetVROM_1K_Bank( page+0, bank*2+0 );
	SetVROM_1K_Bank( page+1, bank*2+1 );
}
//Set VROM 4K Bank
void	SetVROM_4K_Bank( BYTE page, INT bank )
{
	SetVROM_1K_Bank( page+0, bank*4+0 );
	SetVROM_1K_Bank( page+1, bank*4+1 );
	SetVROM_1K_Bank( page+2, bank*4+2 );
	SetVROM_1K_Bank( page+3, bank*4+3 );
}
//Set VROM 8K Bank
void	SetVROM_8K_Bank( INT bank )
{
	for( INT i = 0; i < 8; i++ ) {
		SetVROM_1K_Bank( i, bank*8+i );
	}
}
//Set VROM 8K Bank
void	SetVROM_8K_Bank( INT bank0, INT bank1, INT bank2, INT bank3,
			 INT bank4, INT bank5, INT bank6, INT bank7 )
{
	SetVROM_1K_Bank( 0, bank0 );
	SetVROM_1K_Bank( 1, bank1 );
	SetVROM_1K_Bank( 2, bank2 );
	SetVROM_1K_Bank( 3, bank3 );
	SetVROM_1K_Bank( 4, bank4 );
	SetVROM_1K_Bank( 5, bank5 );
	SetVROM_1K_Bank( 6, bank6 );
	SetVROM_1K_Bank( 7, bank7 );
}
// Set CRAM Bank
void	SetCRAM_1K_Bank( BYTE page, INT bank )
{
	bank &= 0x1F;
	PPU_MEM_BANK[page] = CRAM+0x0400*bank;
	PPU_MEM_TYPE[page] = BANKTYPE_CRAM;
	PPU_MEM_PAGE[page] = bank;

	CRAM_USED[bank>>2] = 0xFF;	// CRAM사용 플래그
}

void	SetCRAM_2K_Bank( BYTE page, INT bank )
{
	SetCRAM_1K_Bank( page+0, bank*2+0 );
	SetCRAM_1K_Bank( page+1, bank*2+1 );
}

void	SetCRAM_4K_Bank( BYTE page, INT bank )
{
	SetCRAM_1K_Bank( page+0, bank*4+0 );
	SetCRAM_1K_Bank( page+1, bank*4+1 );
	SetCRAM_1K_Bank( page+2, bank*4+2 );
	SetCRAM_1K_Bank( page+3, bank*4+3 );
}

void	SetCRAM_8K_Bank( INT bank )
{
	for( INT i = 0; i < 8; i++ ) {
		SetCRAM_1K_Bank( i, bank+i );
	}
}
// Set VRAM Bank , VRAM은 Video RAM이다. http://www.terms.co.kr/VRAM.htm
void	SetVRAM_1K_Bank( BYTE page, INT bank )
{
	bank &= 3;
	PPU_MEM_BANK[page] = VRAM+0x0400*bank;
	PPU_MEM_TYPE[page] = BANKTYPE_VRAM;
	PPU_MEM_PAGE[page] = bank;
}

void	SetVRAM_Bank( INT bank0, INT bank1, INT bank2, INT bank3 )
{
	SetVRAM_1K_Bank(  8, bank0 );
	SetVRAM_1K_Bank(  9, bank1 );
	SetVRAM_1K_Bank( 10, bank2 );
	SetVRAM_1K_Bank( 11, bank3 );
}

void	SetVRAM_Mirror( INT type ) // VRAM Mirror 에 대해서 알아보기..
{
	switch( type ) {
		case	VRAM_HMIRROR:
			SetVRAM_Bank( 0, 0, 1, 1 );
			break;
		case	VRAM_VMIRROR:
			SetVRAM_Bank( 0, 1, 0, 1 );
			break;
		case	VRAM_MIRROR4L:
			SetVRAM_Bank( 0, 0, 0, 0 );
			break;
		case	VRAM_MIRROR4H:
			SetVRAM_Bank( 1, 1, 1, 1 );
			break;
		case	VRAM_MIRROR4:
			SetVRAM_Bank( 0, 1, 2, 3 );
			break;
	}
}

void	SetVRAM_Mirror( INT bank0, INT bank1, INT bank2, INT bank3 )
{
	SetVRAM_1K_Bank(  8, bank0 );
	SetVRAM_1K_Bank(  9, bank1 );
	SetVRAM_1K_Bank( 10, bank2 );
	SetVRAM_1K_Bank( 11, bank3 );
}

