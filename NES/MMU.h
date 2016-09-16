//////////////////////////////////////////////////////////////////////////
//                                                                      //
//      NES Memory Management Unit                                      //
//                                                           Norix      //
//                                               written     2001/02/21 //
//                                               last modify ----/--/-- //
//////////////////////////////////////////////////////////////////////////

#ifndef	__MMU_INCLUDED__	// MMU 관련 데이터 include
#define	__MMU_INCLUDED__

#include "typedef.h"
#include "macro.h"

// CPU 메모리 뱅크
extern	LPBYTE	CPU_MEM_BANK[8];	// 8K 단위 , typedef	BYTE	*LPBYTE;
extern	BYTE	CPU_MEM_TYPE[8];	// typedef	unsigned char	BYTE;
extern	INT	CPU_MEM_PAGE[8];	// 상태 저장용 , typedef	int		BOOL;

// PPU 메모리 뱅크
extern	LPBYTE	PPU_MEM_BANK[12];	// 1KSetPROM_32K_Bank단위
extern	BYTE	PPU_MEM_TYPE[12];
extern	INT	PPU_MEM_PAGE[12];	// 상태저장용
extern	BYTE	CRAM_USED[16];		// 상태저장용


// NES 메모리
extern	BYTE	RAM [  8*1024];		// NES내장 RAM
extern	BYTE	WRAM[128*1024];		// 백업 RAM
extern	BYTE	DRAM[ 40*1024];		// 디스크 시스템 RAM

extern	BYTE	CRAM[ 32*1024];		// 문자 패턴RAM
extern	BYTE	VRAM[  4*1024];		// 이름 테이블 / 속성RAM

extern	BYTE	SPRAM[0x100];		// 스프라이트RAM ( 스프라이트는 영상 속에 작은 2차원 영상이나 애니메이션을 합성하는 기술이다. )
extern	BYTE	BGPAL[0x10];		// BG팔레트
extern	BYTE	SPPAL[0x10];		// SP팔레트

// 레지스터
extern	BYTE	CPUREG[0x18];		// Nes $4000-$4017 (CPU Register 범위) 
extern	BYTE	PPUREG[0x04];		// Nes $2000-$2003 (PPU Register 범위)

// Frame-IRQ레지스터($4017)
extern	BYTE	FrameIRQ;

// PPU내부 레지스터
extern	BYTE	PPU56Toggle;		// $2005-$2006 Toggle
extern	BYTE	PPU7_Temp;		// $2007 read buffer
extern	WORD	loopy_t;		// same as $2005/$2006 , typedef	unsigned short	WORD;
extern	WORD	loopy_v;		// same as $2005/$2006
extern	WORD	loopy_x;		// tile x offset

// ROM데이터 포인터
extern	LPBYTE	PROM;		// PROM ptr
extern	LPBYTE	VROM;		// VROM ptr

#ifdef	_DATATRACE
// For dis...
extern	LPBYTE	PROM_ACCESS;
#endif

// ROM 뱅크 크기
extern	INT	PROM_8K_SIZE, PROM_16K_SIZE, PROM_32K_SIZE;
extern	INT	VROM_1K_SIZE, VROM_2K_SIZE, VROM_4K_SIZE,  VROM_8K_SIZE;


// 함수 선언 부분
extern	void	NesSub_MemoryInitial();

extern	void	SetPROM_Bank( BYTE page, LPBYTE ptr, BYTE type );
extern	void	SetPROM_8K_Bank ( BYTE page, INT bank );
extern	void	SetPROM_16K_Bank( BYTE page, INT bank );
extern	void	SetPROM_32K_Bank( INT bank );					// 오버라이딩
extern	void	SetPROM_32K_Bank( INT bank0, INT bank1, INT bank2, INT bank3 );

extern	void	SetVROM_Bank( BYTE page, LPBYTE ptr, BYTE type );
extern	void	SetVROM_1K_Bank( BYTE page, INT bank );
extern	void	SetVROM_2K_Bank( BYTE page, INT bank );
extern	void	SetVROM_4K_Bank( BYTE page, INT bank );
extern	void	SetVROM_8K_Bank( INT bank );			//오버라이딩
extern	void	SetVROM_8K_Bank( INT bank0, INT bank1, INT bank2, INT bank3,
				 INT bank4, INT bank5, INT bank6, INT bank7 );

extern	void	SetCRAM_1K_Bank( BYTE page, INT bank );
extern	void	SetCRAM_2K_Bank( BYTE page, INT bank );
extern	void	SetCRAM_4K_Bank( BYTE page, INT bank );
extern	void	SetCRAM_8K_Bank( INT bank );

extern	void	SetVRAM_1K_Bank( BYTE page, INT bank );
extern	void	SetVRAM_Bank( INT bank0, INT bank1, INT bank2, INT bank3 );
extern	void	SetVRAM_Mirror( INT type );
extern	void	SetVRAM_Mirror( INT bank0, INT bank1, INT bank2, INT bank3 ); // 미러?


// 메모리 유형
// For PROM (CPU)				// MMU 때문에 0x00이란 주소를 계속 쓸수 있나?
#define	BANKTYPE_ROM	0x00	// PROM(프로그램 가능 판독 전용 메모리)
#define	BANKTYPE_RAM	0xFF
#define	BANKTYPE_DRAM	0x01
#define	BANKTYPE_MAPPER	0x80
// For VROM/VRAM/CRAM (PPU)
#define	BANKTYPE_VROM	0x00
#define	BANKTYPE_CRAM	0x01
#define	BANKTYPE_VRAM	0x80

// 미러 타입?
#define	VRAM_HMIRROR	0x00	// Horizontal
#define	VRAM_VMIRROR	0x01	// Virtical
#define	VRAM_MIRROR4	0x02	// All screen
#define	VRAM_MIRROR4L	0x03	// PA10 L고정 $2000-$23FF 미러
#define	VRAM_MIRROR4H	0x04	// PA10 H고정 $2400-$27FF 미러

#endif	// !__MMU_INCLUDED__

