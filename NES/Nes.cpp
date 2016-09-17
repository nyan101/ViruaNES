#define _CRT_SECURE_NO_WARNINGS 		// scanf warning

//////////////////////////////////////////////////////////////////////////
//                                                                      //
//      NES Emulation core                                              //
//                                                           Norix      //
//                                               written     2001/02/22 //
//                                               last modify ----/--/-- //
//////////////////////////////////////////////////////////////////////////

#define	WIN32_LEAN_AND_MEAN		//WIN32_LEAN_AND_MEAN 매크로를 정의하여 빌드 시간을 단축할 수 있다.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "typedef.h"
#include "macro.h"

#include "VirtuaNESres.h"

#include "DebugOut.h"
#include "App.h"
#include "Pathlib.h"
#include "Config.h"
#include "Crclib.h"

#include "nes.h"
#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "pad.h"
#include "rom.h"
#include "mapper.h"

#include "DirectDraw.h"
#include "DirectSound.h"
#include "DirectInput.h"

NES::NES( const char* fname )	//NES 생성자 각종 변수 초기화 및 하드웨어 변수 할당 체크
{
	DEBUGOUT( "VirtuaNES - NES Emulator for Win32 by Norix (C)2001\n" ); //DEBUGOUT는 Debug output
																		//디버그 출력 창에 하나 이상의 식의 결과를 지시
	bFrameIRQ = TRUE;		//BOOL형	bFrameIRQ(protected)
	FrameIRQ = 0xC0;

	m_bDiskThrottle = FALSE;
	m_CommandRequest = 0;

	m_nSnapNo = 0;

	m_bMoviePlay = m_bMovieRec = FALSE;
	m_fpMovie = NULL;

	// 클래스 prototypes
	cpu = NULL;
	ppu = NULL;
	apu = NULL;
	rom = NULL;
	pad = NULL;
	mapper = NULL;

	SAVERAM_SIZE = 8*1024;	// 8K byte

	// Cheat
	CheatInitial();
	//각종 하드웨어 할당 부분 체크
	try {
		DEBUGOUT( "Allocating CPU..." );
		if( !(cpu = new CPU(this)) )
			throw	"Allocating CPU failed.";
		DEBUGOUT( "Ok.\n" );

		DEBUGOUT( "Allocating PPU..." );
		if( !(ppu = new PPU(this)) )
			throw	"Allocating PPU failed.";
		DEBUGOUT( "Ok.\n" );

		DEBUGOUT( "Allocating APU..." );
		if( !(apu = new APU(this)) )
			throw	"Allocating APU failed.";
		DEBUGOUT( "Ok.\n" );

		DEBUGOUT( "Allocating PAD..." );
		if( !(pad = new PAD(this)) )
			throw	"Allocating PAD failed.";
		DEBUGOUT( "Ok.\n" );

		DEBUGOUT( "Loading ROM Image...\n" );

		if( !(rom = new ROM(fname)) )
			throw	"Allocating ROM failed.";

		if( !(mapper = CreateMapper(this, rom->GetMapperNo())) ) {	//mapper 생성
			// 未サ??トの?ッパ?です
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_UNSUPPORTMAPPER );
			sprintf( szErrorString, szErrStr, rom->GetMapperNo() );
			throw	szErrorString;
		}

		DEBUGOUT( "Ok.\n" );

		DEBUGOUT( "ROM status\n" );
		DEBUGOUT( " %s\n", rom->GetRomName() ); 					//Rom 이름 return
		DEBUGOUT( " Mapper   : %03d\n", rom->GetMapperNo() );		// mapper 번호 return
		DEBUGOUT( " PRG SIZE : %4dK\n", 16*(INT)rom->GetPROM_SIZE() );	//PROM SIZE return
		DEBUGOUT( " CHR SIZE : %4dK\n",  8*(INT)rom->GetVROM_SIZE() );	//VROM SIZE return

		DEBUGOUT( " V MIRROR : " );
		if( rom->IsVMIRROR() ) DEBUGOUT( "Yes\n" );
		else		       DEBUGOUT( "No\n" );
		DEBUGOUT( " 4 SCREEN : " );
		if( rom->Is4SCREEN() ) DEBUGOUT( "Yes\n" );
		else		       DEBUGOUT( "No\n" );
		DEBUGOUT( " SAVE RAM : " );
		if( rom->IsSAVERAM() ) DEBUGOUT( "Yes\n" );
		else		       DEBUGOUT( "No\n" );
		DEBUGOUT( " TRAINER  : " );
		if( rom->IsTRAINER() ) DEBUGOUT( "Yes\n" );
		else		       DEBUGOUT( "No\n" );

		NesSub_MemoryInitial();			// 전체 메모리랑 레지스터 초기화 함수.(mmu.cpp참고)
		LoadSRAM();				//
		LoadDISK();				//

		Reset();
	} catch( CHAR* str ) {		// DELETERPTR => 메모리 할당 취소 시키고 NULL값을 넣음.
		DELETEPTR( cpu );		// #define DELETEPTR(x) if(x) { delete x; x = NULL; } (macro.h참고)
		DELETEPTR( ppu );		// delete 연산자는 메모리 할당 취소시킴.
		DELETEPTR( apu );
		DELETEPTR( pad );
		DELETEPTR( rom );
		DELETEPTR( mapper );
		throw	str;
#ifndef	_DEBUG
	} catch( ... ) {
		DELETEPTR( cpu );
		DELETEPTR( ppu );
		DELETEPTR( apu );
		DELETEPTR( pad );
		DELETEPTR( rom );
		DELETEPTR( mapper );

		// 不明なエラ?が発生しました => 알수없는오류?가 발생하였습니다.
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	DEBUGOUT( "Starting emulation!\n" );
}

NES::~NES()
{
	MovieStop();

	SaveSRAM();
	SaveDISK();

	DEBUGOUT( "Free NES..." );

	DELETEPTR( cpu );
	DELETEPTR( ppu );
	DELETEPTR( apu );
	DELETEPTR( pad );
	DELETEPTR( rom );
	DELETEPTR( mapper );

	DEBUGOUT( "Ok.\n" );
}

// Reset 함수
void	NES::Reset()
{
	SaveSRAM();
	SaveDISK();

	// RAM Clear
	::ZeroMemory( RAM,    8*1024 );
	if( rom->GetPROM_CRC() == 0x29401686 ) {	// Minna no Taabou no Nakayoshi Dai Sakusen(J)
		::memset( RAM, 0xFF, sizeof(RAM) );
	}
//	::memset( RAM, 0x55, sizeof(RAM) );
//	::ZeroMemory( XRAM,   8*1024 );
	::ZeroMemory( CRAM,  32*1024 );		// CRAM 0으로 초기화
	::ZeroMemory( VRAM,   4*1024 );		

	::ZeroMemory( SPRAM, 0x100 );
	::ZeroMemory( BGPAL, 0x10 );
	::ZeroMemory( SPPAL, 0x10 );

	::ZeroMemory( CPUREG, 0x18 );
	::ZeroMemory( PPUREG, 4 );

	bFrameIRQ = TRUE;
	bFrameIRQ_occur = FALSE;
	FrameIRQ = 0xC0;
	FrameIRQ_cycles = FRAMEIRQ_CYCLES;

	m_bDiskThrottle = FALSE;

	SetRenderMethod( PRE_RENDER );

	PROM = rom->GetPROM();
	VROM = rom->GetVROM();

	PROM_8K_SIZE  = rom->GetPROM_SIZE()*2;
	PROM_16K_SIZE = rom->GetPROM_SIZE();
	PROM_32K_SIZE = rom->GetPROM_SIZE()/2;

	VROM_1K_SIZE = rom->GetVROM_SIZE()*8;
	VROM_2K_SIZE = rom->GetVROM_SIZE()*4;
	VROM_4K_SIZE = rom->GetVROM_SIZE()*2;
	VROM_8K_SIZE = rom->GetVROM_SIZE();

	// デフォルトバンク
	if( VROM_8K_SIZE ) {
		SetVROM_8K_Bank( 0 );
	} else {
		SetCRAM_8K_Bank( 0 );
	}

	// ?ラ?
	if( rom->Is4SCREEN() ) {
		SetVRAM_Mirror( VRAM_MIRROR4 );
	} else if( rom->IsVMIRROR() ) {
		SetVRAM_Mirror( VRAM_VMIRROR );
	} else {
		SetVRAM_Mirror( VRAM_HMIRROR );
	}

	apu->SelectExSound( 0 );

	ppu->Reset();
	mapper->Reset();

	// Trainer
	if( rom->IsTRAINER() ) {
		::memcpy( WRAM+0x1000, rom->GetTRAINER(), 512 );
	}

	pad->Reset();
	cpu->Reset();
	apu->Reset();

	base_cycles = emul_cycles = 0;
}

// SoftReset?
void	NES::SoftReset()
{
	pad->Reset();
	cpu->Reset();
	apu->Reset();

	bFrameIRQ_occur = FALSE;
	FrameIRQ = 0xC0;
	FrameIRQ_cycles = FRAMEIRQ_CYCLES;

	m_bDiskThrottle = FALSE;

	base_cycles = emul_cycles = 0;
}

void	NES::EmulationCPU( INT basecycles )
{
INT	cycles;

	base_cycles += basecycles;
	cycles = (INT)((base_cycles/12)-emul_cycles);
	if( cycles > 0 ) {
		emul_cycles += cpu->EXEC( cycles );
	}
}

/*
	?画シ?ケンス
	0		???スキャンライン(?画しない)
	1 - 239		?画
	240		???スキャンライン,VBLANKフラグON
	241		VINT期間,NMI発生
	242-261		VINT期間
	261		VINT期間,VBLANKフラグOFF
*/
void	NES::EmulateFrame( BOOL bDraw )
{
INT	scanline = 0;

	// Cheat
	CheatCodeProcess();

	//
	NES_scanline = scanline;

	if( RenderMethod != TILE_RENDER ) {
		bZapper = FALSE;
		while( TRUE ) {
			ppu->SetRenderScanline( scanline );

			if( scanline == 0 ) {
			// ???スキャンライン
				if( RenderMethod < POST_RENDER ) {
					EmulationCPU( SCANLINE_CYCLES );
					ppu->FrameStart();
					ppu->ScanlineNext();
					mapper->HSync( scanline );
					ppu->ScanlineStart();
				} else {
					EmulationCPU( HDRAW_CYCLES );
					ppu->FrameStart();
					ppu->ScanlineNext();
					mapper->HSync( scanline );
					EmulationCPU( FETCH_CYCLES*32 );
					ppu->ScanlineStart();
					EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
				}
			} else if( scanline < 240 ) {
				if( RenderMethod < POST_RENDER ) {
					if( RenderMethod == POST_ALL_RENDER )
						EmulationCPU( SCANLINE_CYCLES );
					if( bDraw ) {
						ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
					} else {
						if( pad->IsZapperMode() && scanline == ZapperY ) {
							ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
						} else {
							if( !ppu->IsSprite0( scanline ) ) {
								ppu->DummyScanline( scanline );
							} else {
								ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
							}
						}
					}
					ppu->ScanlineNext();				// これの位置でラス??系は画面が違う
					if( RenderMethod == PRE_ALL_RENDER )
						EmulationCPU( SCANLINE_CYCLES );
//					ppu->ScanlineNext();				// これの位置でラス??系は画面が違う
					mapper->HSync( scanline );
					ppu->ScanlineStart();
				} else {
					if( RenderMethod == POST_RENDER )
						EmulationCPU( HDRAW_CYCLES );
					if( bDraw ) {
						ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
					} else {
						if( pad->IsZapperMode() && scanline == ZapperY ) {
							ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
						} else {
							if( !ppu->IsSprite0( scanline ) ) {
								ppu->DummyScanline( scanline );
							} else {
								ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
							}
						}
					}
					if( RenderMethod == PRE_RENDER )
						EmulationCPU( HDRAW_CYCLES );

					ppu->ScanlineNext();
					mapper->HSync( scanline );
					EmulationCPU( FETCH_CYCLES*32 );
					ppu->ScanlineStart();
					EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
				}
			} else if( scanline == 240 ) {
//				// APU同期処理
//				apu->Sync();
				mapper->VSync();

				if( RenderMethod < POST_RENDER ) {
					EmulationCPU( SCANLINE_CYCLES );
					mapper->HSync( scanline );
				} else {
					EmulationCPU( HDRAW_CYCLES );
					mapper->HSync( scanline );
					EmulationCPU( HBLANK_CYCLES );
				}
			} else if( scanline <= 261 ) {
				// VBLANK期間(Scanline 241?261)
				if( scanline == 261 ) {
					ppu->VBlankEnd();
				}
				if( RenderMethod < POST_RENDER ) {
					if( scanline == 241 ) {
						ppu->VBlankStart();
						EmulationCPU( 4*12 );
						if( PPUREG[0]&PPU_VBLANK_BIT ) {
							cpu->NMI();
						}
						EmulationCPU( SCANLINE_CYCLES-4*12 );
					} else {
						EmulationCPU( SCANLINE_CYCLES );
					}
					mapper->HSync( scanline );
				} else {
					if( scanline == 241 ) {
						ppu->VBlankStart();
						EmulationCPU( 4*12 );
						if( PPUREG[0]&PPU_VBLANK_BIT ) {
							cpu->NMI();
						}
						EmulationCPU( HDRAW_CYCLES-4*12 );
					} else {
						EmulationCPU( HDRAW_CYCLES );
					}
					mapper->HSync( scanline );
					EmulationCPU( HBLANK_CYCLES );
				}

				if( scanline == 261 ) {
//					ppu->VBlankEnd();
					break;
				}
			}
			if( pad->IsZapperMode() ) {
				if( scanline == ZapperY )
					bZapper = TRUE;
				else
					bZapper = FALSE;
			}

			scanline++;
			NES_scanline = scanline;
		}
	} else {
		while( TRUE ) {
			ppu->SetRenderScanline( scanline );

			if( scanline == 0 ) {
			// ???スキャンライン
				// H-Draw (4fetches*32)
				EmulationCPU( FETCH_CYCLES*128 );
				ppu->FrameStart();
				ppu->ScanlineNext();
				EmulationCPU( FETCH_CYCLES*16 );
				mapper->HSync( scanline );
				EmulationCPU( FETCH_CYCLES*16 );
				ppu->ScanlineStart();
				EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
			} else if( scanline < 240 ) {
			// スクリ?ン?画(Scanline 1?239)
				if( bDraw ) {
					ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
					ppu->ScanlineNext();
					EmulationCPU( FETCH_CYCLES*16 );
					mapper->HSync( scanline );
					EmulationCPU( FETCH_CYCLES*16 );
					ppu->ScanlineStart();
					EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
				} else {
					if( pad->IsZapperMode() && scanline == ZapperY ) {
						ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
					} else {
						if( !ppu->IsSprite0( scanline ) ) {
							// H-Draw (4fetches*32)
							EmulationCPU( FETCH_CYCLES*128 );
							ppu->DummyScanline( scanline );
							ppu->ScanlineNext();
							EmulationCPU( FETCH_CYCLES*16 );
							mapper->HSync( scanline );
							EmulationCPU( FETCH_CYCLES*16 );
							ppu->ScanlineStart();
							EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
						} else {
							EmulationCPU( HDRAW_CYCLES );
							ppu->Scanline( scanline, Config.graphics.bAllSprite, Config.graphics.bLeftClip );
							ppu->ScanlineNext();
							EmulationCPU( FETCH_CYCLES*16 );
							mapper->HSync( scanline );
							EmulationCPU( FETCH_CYCLES*16 );
							ppu->ScanlineStart();
							EmulationCPU( FETCH_CYCLES*10+SCANLINE_END_CYCLES );
						}
					}
				}
			} else if( scanline == 240 ) {
			// ???スキャンライン (Scanline 240)
//				// APU同期処理
//				apu->Sync();
				mapper->VSync();

				EmulationCPU( HDRAW_CYCLES );
				// H-Sync
				mapper->HSync( scanline );

				EmulationCPU( HBLANK_CYCLES );
			} else if( scanline <= 261 ) {
			// VBLANK期間(Scanline 241?261)
				if( scanline == 261 ) {
					ppu->VBlankEnd();
				}
				if( scanline == 241 ) {
					ppu->VBlankStart();
					if( PPUREG[0]&PPU_VBLANK_BIT ) {
						EmulationCPU( 4*12 );
						cpu->NMI();
						EmulationCPU( HDRAW_CYCLES-4*12 );
					} else {
						EmulationCPU( HDRAW_CYCLES );
					}
				} else {
					EmulationCPU( HDRAW_CYCLES );
				}

				// H-Sync
				mapper->HSync( scanline );

				EmulationCPU( HBLANK_CYCLES );

				if( scanline == 261 ) {
//					ppu->VBlankEnd();
					break;
				}
			}
			if( pad->IsZapperMode() ) {
				if( scanline == ZapperY )
					bZapper = TRUE;
				else
					bZapper = FALSE;
			}

			scanline++;
			NES_scanline = scanline;
		}
	}
}

void	NES::Clock( INT cycles )
{
	if( (FrameIRQ_cycles-=cycles) < 0 ) {
		FrameIRQ_cycles += FRAMEIRQ_CYCLES;
		bFrameIRQ_occur = TRUE;
		// APU同期処理
		apu->Sync();
	}

	if( !(FrameIRQ & 0xC0) && bFrameIRQ ) {
		if( bFrameIRQ_occur ) {
			cpu->IRQ_NotPending();
		}
	}
}

BYTE	NES::Read( WORD addr )
{
	switch( addr>>13 ) {
		case	0x00:	// $0000-$1FFF
			return	RAM[addr&0x07FF];
		case	0x01:	// $2000-$3FFF
			return	ppu->Read( addr&0xE007 );
		case	0x02:	// $4000-$5FFF
			if( addr < 0x4100 ) {
				return	ReadReg( addr );
			} else {
				return	mapper->ReadLow( addr );
			}
			break;
		case	0x03:	// $6000-$7FFF
			return	mapper->ReadLow( addr );
		case	0x04:	// $8000-$9FFF
		case	0x05:	// $A000-$BFFF
		case	0x06:	// $C000-$DFFF
		case	0x07:	// $E000-$FFFF
			return	CPU_MEM_BANK[addr>>13][addr&0x1FFF];
	}

	return	0x00;	// Warning?防
}

void	NES::Write( WORD addr, BYTE data )
{
	switch( addr>>13 ) {
		case	0x00:	// $0000-$1FFF
			RAM[addr&0x07FF] = data;
			break;
		case	0x01:	// $2000-$3FFF
			ppu->Write( addr&0xE007, data );
			break;
		case	0x02:	// $4000-$5FFF
			if( addr < 0x4100 ) {
				WriteReg( addr, data );
			} else {
				mapper->WriteLow( addr, data );
			}
			break;
		case	0x03:	// $6000-$7FFF
			mapper->WriteLow( addr, data );
			break;
		case	0x04:	// $8000-$9FFF
		case	0x05:	// $A000-$BFFF
		case	0x06:	// $C000-$DFFF
		case	0x07:	// $E000-$FFFF
			mapper->Write( addr, data );

			GenieCodeProcess();
			break;
	}
}

BYTE	NES::ReadReg( WORD addr )
{
BYTE	data = 0;

	switch( addr & 0xFF ) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0A: case 0x0B:
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
		case 0x10: case 0x11: case 0x12: case 0x13:
			return	apu->Read( addr );
			break;
		case	0x15:
			data = apu->Read( addr )|(bFrameIRQ_occur?0x40:0x00);
			bFrameIRQ_occur = FALSE;
			return	data;
			break;

		case	0x14:
			return	addr&0xFF;
			break;

		case	0x16:
			return	pad->Read(addr) | 0x40;
			break;
		case	0x17:
			return	pad->Read(addr) | (bFrameIRQ_occur?0x00:0x40);
			break;
		default:
			return	mapper->ExRead( addr );
			break;
	}
}

void	NES::WriteReg( WORD addr, BYTE data )
{
BYTE	reg = (BYTE)addr;

	switch( addr & 0xFF ) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0A: case 0x0B:
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x15:
			apu->Write( addr, data );
			CPUREG[reg] = data;
			break;

		case	0x14:
			ppu->DMA( data );
//////			cpu->DMA( 512 ); // DMA Pending cycle
			cpu->DMA( 514 ); // DMA Pending cycle
			CPUREG[reg] = data;
			break;

		case	0x16:
			pad->Write( addr, data );
			CPUREG[reg] = data;
			break;
		case	0x17:
			FrameIRQ_cycles = FRAMEIRQ_CYCLES;
			FrameIRQ = data & 0xC0;
			bFrameIRQ_occur = FALSE;
			CPUREG[reg] = data;

			// 強制同期…っぽい
			if( data & 0x80 ) {
				apu->Sync();
			}

			pad->Write( addr, data );
			apu->Write( addr, data );
			break;
		default:
			mapper->ExWrite( addr, data );
			break;
	}
}

void	NES::LoadSRAM()
{
	if( rom->IsNSF() )
		return;

	::ZeroMemory( WRAM, sizeof(WRAM) );

	if( !rom->IsSAVERAM() )
		return;

	string	tempstr;
	if( Config.path.bSavePath ) {
		tempstr = CPathlib::MakePathExt( Config.path.szSavePath, rom->GetRomName(), "sav" );
	} else {
		tempstr = CPathlib::MakePathExt( rom->GetRomPath(), rom->GetRomName(), "sav" );
	}
	DEBUGOUT( "Path: %s\n", tempstr.c_str() );

	FILE*	fp = NULL;
	try
	{
		if( !(fp = ::fopen( tempstr.c_str(), "rb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, tempstr.c_str() );
			throw	szErrorString;
		}

		DEBUGOUT( "Loading SAVERAM..." );

		LONG	size;
		// フ?イルサイズ取得
		::fseek( fp, 0, SEEK_END );
		size = ftell( fp );
		::fseek( fp, 0, SEEK_SET );
		if( size <= 128*1024 ) {
			if( ::fread( WRAM, size, 1, fp ) != 1 )
				throw	"File Read error.";
		}

		DEBUGOUT( "Ok.\n" );
		FCLOSE( fp );
	} catch( CHAR* str ) {
		FCLOSE( fp );
		DEBUGOUT( "Loading SAVERAM Error.\n" );
		DEBUGOUT( "%s\n", str );
//		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		FCLOSE( fp );
		DEBUGOUT( "Loading SAVERAM Error.\n" );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}
}

void	NES::SaveSRAM()
{
INT	i;

	if( rom->IsNSF() )
		return;

	if( !rom->IsSAVERAM() )
		return;

	for( i = 0; i < SAVERAM_SIZE; i++ ) {
		if( WRAM[i] != 0x00 )
			break;
	}

	if( i < SAVERAM_SIZE ) {
		DEBUGOUT( "Saving SAVERAM...\n" );

		string	tempstr;
		if( Config.path.bSavePath ) {
			CreateDirectory( Config.path.szSavePath, NULL );
			tempstr = CPathlib::MakePathExt( Config.path.szSavePath, rom->GetRomName(), "sav" );
		} else {
			tempstr = CPathlib::MakePathExt( rom->GetRomPath(), rom->GetRomName(), "sav" );
		}
		DEBUGOUT( "Path: %s\n", tempstr.c_str() );

		FILE*	fp = NULL;
		try
		{
			if( !(fp = ::fopen( tempstr.c_str(), "wb" )) ) {
				// xxx フ?イルを開けません
				LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
				sprintf( szErrorString, szErrStr, tempstr.c_str() );
				throw	szErrorString;
			}

			if( ::fwrite( WRAM, SAVERAM_SIZE, 1, fp ) != 1 ) {
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}

			DEBUGOUT( "Ok.\n" );
			FCLOSE( fp );
		} catch( CHAR* str ) {
			DEBUGOUT( "Writing SAVERAM Error.\n" );
			FCLOSE( fp );
			throw	str;
	#ifndef	_DEBUG
		} catch(...) {
			DEBUGOUT( "Writing SAVERAM Error.\n" );
			FCLOSE( fp );
			// 不明なエラ?が発生しました
			throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
	#endif
		}
	}
}

void	NES::LoadDISK()
{
	if( rom->GetMapperNo() != 20 )
		return;

	BOOL	bExit = FALSE;

	INT	i, j, diskno;
	FILE*	fp = NULL;
	DISKIMGFILEHDR	ifh;
	DISKIMGHDR	hdr;
	LPBYTE		disk;

	WORD	Version;

	string	tempstr;
	if( Config.path.bSavePath ) {
		tempstr = CPathlib::MakePathExt( Config.path.szSavePath, rom->GetRomName(), "dsv" );
	} else {
		tempstr = CPathlib::MakePathExt( rom->GetRomPath(), rom->GetRomName(), "dsv" );
	}
	DEBUGOUT( "Path: %s\n", tempstr.c_str() );

	try
	{
		if( !(fp = ::fopen( tempstr.c_str(), "rb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, tempstr.c_str() );
			throw	szErrorString;
		}

		if( ::fread( &ifh, sizeof(DISKIMGFILEHDR), 1, fp ) != 1 ) {
			// フ?イルの読み込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_READ );
		}

		if( ::memcmp( ifh.ID, "VirtuaNES DI", sizeof(ifh.ID) ) == 0 ) {
			if( ifh.BlockVersion < 0x0100 && ifh.BlockVersion > 0x200 ) {
				// 未対応?式です
				throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
			}
			Version = ifh.BlockVersion;
		} else {
			// 未対応?式です
			throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
		}

		if( Version == 0x0100 ) {
		// Ver0.24以前
			if( ifh.DiskNumber > 4 ) {
				// 未対応?式です
				throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
			}

			for( i = 0; i < (INT)ifh.DiskNumber; i++ ) {
				if( ::fread( &hdr, sizeof(DISKIMGHDR), 1, fp ) != 1 ) {
					if( i == 0 ) {
						// フ?イルの読み込みに失敗しました
						throw	CApp::GetErrorString( IDS_ERROR_READ );
					} else {
						break;
					}
				}

				if( ::memcmp( hdr.ID, "SIDE0A", sizeof(hdr.ID) ) == 0 ) {
					diskno = 0;
				} else if( ::memcmp( hdr.ID, "SIDE0B", sizeof(hdr.ID) ) == 0 ) {
					diskno = 1;
				} else if( ::memcmp( hdr.ID, "SIDE1A", sizeof(hdr.ID) ) == 0 ) {
					diskno = 2;
				} else if( ::memcmp( hdr.ID, "SIDE1B", sizeof(hdr.ID) ) == 0 ) {
					diskno = 3;
				} else {
					// 未対応?式です
					throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
				}

				for( j = 0; j < 16; j++ ) {
					if( hdr.DiskTouch[j] ) {
						disk = rom->GetPROM()+16+65500*diskno+(4*1024)*j;
						if( j < 15 ) {
							if( ::fread( disk, 4*1024, 1, fp ) != 1 ) {
								bExit = TRUE;
								// フ?イルの読み込みに失敗しました
								throw	CApp::GetErrorString( IDS_ERROR_READ );
							}
						} else {
							if( ::fread( disk, 4*1024-36, 1, fp ) != 1 ) {
								bExit = TRUE;
								// フ?イルの読み込みに失敗しました
								throw	CApp::GetErrorString( IDS_ERROR_READ );
							}
						}
					}
				}
			}
		} else 
		if( Version == 0x0200 || Version == 0x0210 ) {
			// Ver0.30以?
			DISKFILEHDR	dfh;
			LPBYTE	lpDisk = rom->GetPROM();
			LPBYTE	lpWrite = rom->GetDISK();
			LONG	DiskSize = 16+65500*rom->GetDiskNo();
			DWORD	pos;
			BYTE	data;

			// 書き換えFLAG消去
			::ZeroMemory( lpWrite, 16+65500*rom->GetDiskNo() );

			// ヘッ?読み直し
			if( ::fseek( fp, 0, SEEK_SET ) ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}
			if( ::fread( &dfh, sizeof(DISKFILEHDR), 1, fp ) != 1 ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}

			if( Config.emulator.bCrcCheck ) {
				// 現在ロ?ド中の?イトルと違うかを?ェック
				if( dfh.ProgID  !=       rom->GetGameID()
				 || dfh.MakerID != (WORD)rom->GetMakerID()
				 || dfh.DiskNo  != (WORD)rom->GetDiskNo() ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
			}

			for( i = 0; i < dfh.DifferentSize; i++ ) {
				if( ::fread( &pos, sizeof(DWORD), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					bExit = TRUE;
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
				data = (BYTE)(pos>>24);
				pos &= 0x00FFFFFF;
				if( pos >= 16 && pos < DiskSize ) {
					lpDisk[pos] = data;
					lpWrite[pos] = 0xFF;
				}
			}
		}
		FCLOSE( fp );
	} catch( CHAR* str ) {
		FCLOSE( fp );
		DEBUGOUT( "%s\n", str );
		if( bExit )
			throw	str;
#ifndef	_DEBUG
	} catch(...) {
		FCLOSE( fp );
		DEBUGOUT( "Loading DISKIMAGE Error.\n" );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}
}

void	NES::SaveDISK()
{
	if( rom->GetMapperNo() != 20 )
		return;

	INT	i;
	FILE*	fp = NULL;
	DISKFILEHDR ifh;
	LPBYTE	lpDisk  = rom->GetPROM();
	LPBYTE	lpWrite = rom->GetDISK();
	LONG	DiskSize = 16+65500*rom->GetDiskNo();
	DWORD	data;

	try
	{
		::ZeroMemory( &ifh, sizeof(ifh) );

		::memcpy( ifh.ID, "VirtuaNES DI", sizeof(ifh.ID) );
		ifh.BlockVersion = 0x0210;
		ifh.ProgID  = rom->GetGameID();
		ifh.MakerID = (WORD)rom->GetMakerID();
		ifh.DiskNo  = (WORD)rom->GetDiskNo();

		// 相違数をカウント
		for( i = 16; i < DiskSize; i++ ) {
			if( lpWrite[i] )
				ifh.DifferentSize++;
		}

		if( !ifh.DifferentSize )
			return;

		string	tempstr;
		if( Config.path.bSavePath ) {
			::CreateDirectory( Config.path.szSavePath, NULL );
			tempstr = CPathlib::MakePathExt( Config.path.szSavePath, rom->GetRomName(), "dsv" );
		} else {
			tempstr = CPathlib::MakePathExt( rom->GetRomPath(), rom->GetRomName(), "dsv" );
		}
		DEBUGOUT( "Path: %s\n", tempstr.c_str() );

		if( !(fp = ::fopen( tempstr.c_str(), "wb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			::wsprintf( szErrorString, szErrStr, tempstr.c_str() );
			throw	szErrorString;
		}

		if( ::fwrite( &ifh, sizeof(DISKFILEHDR), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}

		for( i = 16; i < DiskSize; i++ ) {
			if( lpWrite[i] ) {
				data = i & 0x00FFFFFF;
				data |= ((DWORD)lpDisk[i]&0xFF)<<24;

				// Write File
				if( ::fwrite( &data, sizeof(DWORD), 1, fp ) != 1 ) {
					// フ?イルの書き込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_WRITE );
				}
			}
		}
		FCLOSE( fp );
	} catch( CHAR* str ) {
		FCLOSE( fp );
		DEBUGOUT( "%s\n", str );
#ifndef	_DEBUG
	} catch(...) {
		FCLOSE( fp );
		DEBUGOUT( "Saving DISKIMAGE Error.\n" );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}
}

BOOL	NES::IsStateFile( const char* fname )
{
FILE*	fp = NULL;
FILEHDR	header;

	if( !(fp = ::fopen( fname, "rb" )) )
		return	FALSE;

	if( ::fread( &header, sizeof(header), 1, fp ) != 1 ) {
		FCLOSE( fp );
		return	FALSE;
	}
	FCLOSE( fp );

	if( ::memcmp( header.ID, "VirtuaNES ST", sizeof(header.ID) ) == 0 ) {
		if( header.BlockVersion >= 0x0100 )
			return	TRUE;
	}

	return	FALSE;
}

BOOL	NES::LoadState( const char* fname )
{
FILE*	fp = NULL;
BOOL	bRet = FALSE;

	if( rom->IsNSF() )
		return	TRUE;

	try {
		if( !(fp = ::fopen( fname, "rb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, fname );
			throw	szErrorString;
		}

		bRet = ReadState( fp );

		FCLOSE( fp );
	} catch( CHAR* str ) {
		DEBUGOUT( "State load error.\n" );
		DEBUGOUT( "%s\n", str );
		FCLOSE( fp );
		return	FALSE;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "State load error.\n" );
		FCLOSE( fp );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	bRet;
}

BOOL	NES::SaveState( const char* fname )
{
FILE*	fp = NULL;

	if( rom->IsNSF() )
		return	TRUE;

	try {
		if( !(fp = ::fopen( fname, "wb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, fname );
			throw	szErrorString;
		}

		WriteState( fp );

		FCLOSE( fp );
	} catch( CHAR* str ) {
		DEBUGOUT( "State save error.\n" );
		FCLOSE( fp );
		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "State save error.\n" );
		FCLOSE( fp );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	TRUE;
}

BOOL	NES::ReadState( FILE* fp )
{
	INT	i;
	BOOL	bHeader = FALSE;
	WORD	Version = 0;

	BLOCKHDR hdr;
	INT	type;

	while( TRUE ) {
		// Read File
		if( ::fread( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 )
			break;

		// File Header check
		if( !bHeader ) {
			LPFILEHDR	fh = (LPFILEHDR)&hdr;
			if( ::memcmp( fh->ID, "VirtuaNES ST", sizeof(fh->ID) ) == 0 ) {
				Version = fh->BlockVersion;
				if( Version == 0x0100 ) {
				// Ver0.24まで
					bHeader = TRUE;
					// 古い奴は??ビ?中はロ?ド出来ません
					if( m_bMoviePlay ) {
						return	FALSE;
					}
					// 古い奴のFDSはロ?ド出来ません
					if( rom->GetMapperNo() == 20 ) {
						// 未対応?式です
						throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
					}
				} else 
				if( Version == 0x0200 ) {
				// Ver0.30以?
					FILEHDR2 hdr2;
					// ヘッ?部読み直し
					if( ::fseek( fp, -sizeof(BLOCKHDR), SEEK_CUR ) ) {
						// フ?イルの読み込みに失敗しました
						throw	CApp::GetErrorString( IDS_ERROR_READ );
					}
					// Read File
					if( ::fread( &hdr2, sizeof(FILEHDR2), 1, fp ) != 1 ) {
						// フ?イルの読み込みに失敗しました
						throw	CApp::GetErrorString( IDS_ERROR_READ );
					}

					if( Config.emulator.bCrcCheck ) {
						// 現在ロ?ド中の?イトルと違うかを?ェック
						if( rom->GetMapperNo() != 20 ) {
						// FDS以外
							if( hdr2.Ext0 != rom->GetPROM_CRC() ) {
								return	FALSE;	// 違うじゃん
							}
						} else {
						// FDS
							if( hdr2.Ext0 != rom->GetGameID() ||
							    hdr2.Ext1 != (WORD)rom->GetMakerID() ||
							    hdr2.Ext2 != (WORD)rom->GetDiskNo() )
								return	FALSE;	// 違うじゃん
						}
					}

					// ??ビ?中はフ?イル?イン?とステップ数を
					// 変更して，??ビ?を記?モ?ドに変更
					if( m_bMoviePlay || m_bMovieRec ) {
						// 撮り直し可?？
						if( m_hedMovie.Control & 0x80 ) {
							if( hdr2.MovieOffset && hdr2.MovieStep ) {
								if( m_bMoviePlay ) {
								// 再生中
									// ステ?トのステップ数が記?より進んでたら?メ
									if( hdr2.MovieStep > m_hedMovie.MovieStep )
										return	FALSE;
								} else {
								// 記?中
									// ステ?トのステップ数が現在より進んでたら?メ
									if( hdr2.MovieStep > m_MovieStep )
										return	FALSE;
								}

DEBUGOUT( "LD STEP=%d POS=%d\n", hdr2.MovieStep, hdr2.MovieOffset );

								m_bMoviePlay = FALSE;
								m_bMovieRec = TRUE;
								m_MovieStep = hdr2.MovieStep;
								m_hedMovie.RecordTimes++;	// 撮り直し回数+1
								if( ::fseek( m_fpMovie, hdr2.MovieOffset, SEEK_SET ) ) {
DEBUGOUT( "MOVIE:STATE LOAD SEEK 失敗\n" );
									// フ?イルの読み込みに失敗しました
									throw	CApp::GetErrorString( IDS_ERROR_READ );
								}
							} else {
								return	FALSE;
							}
						} else {
							return	FALSE;
						}
					}
				}
				bHeader = TRUE;
				continue;
			}
		}

		if( !bHeader ) {
			// 未対応?式です
			throw	CApp::GetErrorString( IDS_ERROR_UNSUPPORTFORMAT );
		}

//DEBUGOUT( "HEADER ID=%8s\n", hdr.ID );

		type = -1;
		if( ::memcmp( hdr.ID, "REG DATA", sizeof(hdr.ID) ) == 0 ) 
			type = 0;
		if( ::memcmp( hdr.ID, "RAM DATA", sizeof(hdr.ID) ) == 0 )
			type = 1;
		if( ::memcmp( hdr.ID, "MMU DATA", sizeof(hdr.ID) ) == 0 )
			type = 2;
		if( ::memcmp( hdr.ID, "MMC DATA", sizeof(hdr.ID) ) == 0 )
			type = 3;
		if( ::memcmp( hdr.ID, "CONTDATA", sizeof(hdr.ID) ) == 0 )
			type = 4;

		if( rom->GetMapperNo() == 20 ) {
			if( ::memcmp( hdr.ID, "DISKDATA", sizeof(hdr.ID) ) == 0 )
				type = 5;
		}

		if( type == -1 ) {
//DEBUGOUT( "UNKNOWN HEADER ID=%8s\n", hdr.ID );
			break;
		}

		switch( type ) {
			case	0:
				// REGISTER STATE
				{
				REGSTAT	reg;

				if( ::fread( &reg, sizeof(REGSTAT), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}

				// LOAD CPU STATE
				R6502	R;
				R.PC = reg.cpureg.cpu.PC;
				R.A  = reg.cpureg.cpu.A;
				R.X  = reg.cpureg.cpu.X;
				R.Y  = reg.cpureg.cpu.Y;
				R.S  = reg.cpureg.cpu.S;
				R.P  = reg.cpureg.cpu.P;
				R.INT_pending = reg.cpureg.cpu.I;
				cpu->SetContext( R );
				FrameIRQ = reg.cpureg.cpu.FrameIRQ;

				emul_cycles = 0;
				base_cycles = reg.cpureg.cpu.mod_cycles;

				// LOAD PPU STATE
				PPUREG[0] = reg.ppureg.ppu.reg0;
				PPUREG[1] = reg.ppureg.ppu.reg1;
				PPUREG[2] = reg.ppureg.ppu.reg2;
				PPUREG[3] = reg.ppureg.ppu.reg3;
				PPU7_Temp = reg.ppureg.ppu.reg7;
				loopy_t = reg.ppureg.ppu.loopy_t;
				loopy_v = reg.ppureg.ppu.loopy_v;
				loopy_x = reg.ppureg.ppu.loopy_x;
				PPU56Toggle = reg.ppureg.ppu.toggle56;

				// SAVE APU STATE
				for( i = 0; i < 0x18; i++ ) {
					CPUREG[i] = reg.apu.reg[i];
					if( i < 0x10 && i == 0x15 ) {
						apu->Write( 0x4000+i, CPUREG[i] );
					}
				}
				// DMCは?めないとまずい事が起こる
				for( i = 0x4010; i <= 0x4013; i++ ) {
					apu->Write( i, 0 );
				}
				}
				break;
			case	1:
				// RAM STATE
				{
				RAMSTAT	ram;
				if( ::fread( &ram, sizeof(RAMSTAT), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
				::memcpy( RAM, ram.RAM, sizeof(ram.RAM) );
				::memcpy( BGPAL, ram.BGPAL, sizeof(ram.BGPAL) );
				::memcpy( SPPAL, ram.SPPAL, sizeof(ram.SPPAL) );
				::memcpy( SPRAM, ram.SPRAM, sizeof(ram.SPRAM) );
				if( rom->IsSAVERAM() ) {
					if( ::fread( WRAM, SAVERAM_SIZE, 1, fp ) != 1 ) {
						// フ?イルの読み込みに失敗しました
						throw	CApp::GetErrorString( IDS_ERROR_READ );
					}
				}
				}
				break;
			case	2:
				// BANK STATE
				{
				MMUSTAT mmu;
				if( ::fread( &mmu, sizeof(MMUSTAT), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
				if( hdr.BlockVersion == 0x100 ) {
				// ちょっと前のバ?ジョン
					if( mmu.CPU_MEM_TYPE[3] == BANKTYPE_RAM
					 || mmu.CPU_MEM_TYPE[3] == BANKTYPE_DRAM ) {
						if( ::fread( CPU_MEM_BANK[3], 8*1024, 1, fp ) != 1 ) {
							// フ?イルの読み込みに失敗しました
							throw	CApp::GetErrorString( IDS_ERROR_READ );
						}
					} else if( !rom->IsSAVERAM() ) {
						SetPROM_8K_Bank( 3, mmu.CPU_MEM_PAGE[3] );
					}
					// バンク0?3以外ロ?ド
					for( i = 4; i < 8; i++ ) {
						CPU_MEM_TYPE[i] = mmu.CPU_MEM_TYPE[i];
						CPU_MEM_PAGE[i] = mmu.CPU_MEM_PAGE[i];
						if( CPU_MEM_TYPE[i] == BANKTYPE_ROM ) {
							SetPROM_8K_Bank( i, CPU_MEM_PAGE[i] );
						} else {
							if( ::fread( CPU_MEM_BANK[i], 8*1024, 1, fp ) != 1 ) {
								// フ?イルの読み込みに失敗しました
								throw	CApp::GetErrorString( IDS_ERROR_READ );
							}
						}
					}
				} else if( hdr.BlockVersion == 0x200 ) {
				// 最新バ?ジョン
					// SRAMがあっても全部ロ?ドしなおし
					for( i = 3; i < 8; i++ ) {
						CPU_MEM_TYPE[i] = mmu.CPU_MEM_TYPE[i];
						CPU_MEM_PAGE[i] = mmu.CPU_MEM_PAGE[i];
						if( CPU_MEM_TYPE[i] == BANKTYPE_ROM ) {
							SetPROM_8K_Bank( i, CPU_MEM_PAGE[i] );
						} else {
							if( ::fread( CPU_MEM_BANK[i], 8*1024, 1, fp ) != 1 ) {
								// フ?イルの読み込みに失敗しました
								throw	CApp::GetErrorString( IDS_ERROR_READ );
							}
						}
					}
				}
				// VRAM
				if( ::fread( VRAM, 4*1024, 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}

				// CRAM
				for( i = 0; i < 8; i++ ) {
					if( mmu.CRAM_USED[i] != 0 ) {
						if( ::fread( &CRAM[0x1000*i], 4*1024, 1, fp ) != 1 ) {
							// フ?イルの読み込みに失敗しました
							throw	CApp::GetErrorString( IDS_ERROR_READ );
						}
					}
				}
				// BANK
				for( i = 0; i < 12; i++ ) {
					if( mmu.PPU_MEM_TYPE[i] == BANKTYPE_VROM ) {
						SetVROM_1K_Bank( i, mmu.PPU_MEM_PAGE[i] );
					} else if( mmu.PPU_MEM_TYPE[i] == BANKTYPE_CRAM ) {
						SetCRAM_1K_Bank( i, mmu.PPU_MEM_PAGE[i] );
					} else if( mmu.PPU_MEM_TYPE[i] == BANKTYPE_VRAM ) {
						SetVRAM_1K_Bank( i, mmu.PPU_MEM_PAGE[i] );
					} else {
						throw	"Unknown bank types.";
					}
				}
				}
				break;
			case	3:
				// MMC STATE
				{
				MMCSTAT	mmc;
				if( ::fread( &mmc, sizeof(MMCSTAT), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
				mapper->LoadState( mmc.mmcdata );
				}
				break;
			case	4:
				break;

			// Disk Images
			// Ver0.30以?
			case	5:
				{
				DISKDATA ddata;
				DWORD	pos;
				BYTE	data;
				LONG	DiskSize = 16+65500*rom->GetDiskNo();
				LPBYTE	lpDisk  = rom->GetPROM();
				LPBYTE	lpWrite = rom->GetDISK();


				// 書き換えFLAG消去
				::ZeroMemory( lpWrite, 16+65500*rom->GetDiskNo() );

				if( ::fread( &ddata, sizeof(DISKDATA), 1, fp ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}

				for( i = 0; i < ddata.DifferentSize; i++ ) {
					if( ::fread( &pos, sizeof(DWORD), 1, fp ) != 1 ) {
						// フ?イルの読み込みに失敗しました
						throw	CApp::GetErrorString( IDS_ERROR_READ );
					}
					data = (BYTE)(pos>>24);
					pos &= 0x00FFFFFF;
					if( pos >= 16 && pos < DiskSize ) {
						lpDisk[pos] = data;
						lpWrite[pos] = 0xFF;
					}
				}

				}
				break;
		}
	}

	return	TRUE;
}

void	NES::WriteState( FILE* fp )
{
	INT	i;

	// HEADER
	{
	FILEHDR2 hdr;

	::ZeroMemory( &hdr, sizeof(FILEHDR) );
	::memcpy( hdr.ID, "VirtuaNES ST", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0200;

	if( rom->GetMapperNo() != 20 ) {
		hdr.Ext0 = rom->GetPROM_CRC();
	} else {
		hdr.Ext0 = rom->GetGameID();
		hdr.Ext1 = (WORD)rom->GetMakerID();
		hdr.Ext2 = (WORD)rom->GetDiskNo();
	}

	// ??ビ?再生や記?中であればその位置を記?する
	if( m_bMoviePlay || m_bMovieRec ) {
		hdr.MovieStep   = m_MovieStep;
		hdr.MovieOffset = ::ftell( m_fpMovie );
DEBUGOUT( "\nSV STEP=%d POS=%d\n", m_MovieStep, hdr.MovieOffset );
	}

	// Write File
	if( ::fwrite( &hdr, sizeof(FILEHDR2), 1, fp ) != 1 )
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}

	BLOCKHDR hdr;

	// REGISTER STATE
	{
	REGSTAT	reg;

	::ZeroMemory( &hdr, sizeof(BLOCKHDR) );
	::ZeroMemory( &reg, sizeof(REGSTAT) );

	// Create Header
	::memcpy( hdr.ID, "REG DATA", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0100;
	hdr.BlockSize    = sizeof(REGSTAT);

	// SAVE CPU STATE
	R6502	R;
	cpu->GetContext( R );

	reg.cpureg.cpu.PC = R.PC;
	reg.cpureg.cpu.A  = R.A;
	reg.cpureg.cpu.X  = R.X;
	reg.cpureg.cpu.Y  = R.Y;
	reg.cpureg.cpu.S  = R.S;
	reg.cpureg.cpu.P  = R.P;
	reg.cpureg.cpu.I  = R.INT_pending;

	reg.cpureg.cpu.FrameIRQ = FrameIRQ;

	reg.cpureg.cpu.mod_cycles = (LONG)(base_cycles-emul_cycles*12);

	// SAVE PPU STATE
	reg.ppureg.ppu.reg0 = PPUREG[0];
	reg.ppureg.ppu.reg1 = PPUREG[1];
	reg.ppureg.ppu.reg2 = PPUREG[2];
	reg.ppureg.ppu.reg3 = PPUREG[3];
	reg.ppureg.ppu.reg7 = PPU7_Temp;
	reg.ppureg.ppu.loopy_t  = loopy_t;
	reg.ppureg.ppu.loopy_v  = loopy_v;
	reg.ppureg.ppu.loopy_x  = loopy_x;
	reg.ppureg.ppu.toggle56 = PPU56Toggle;

	// SAVE APU STATE
	for( i = 0; i < 0x18; i++ ) {
		reg.apu.reg[i] = CPUREG[i];
	}

	// Write File
	if( ::fwrite( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	if( ::fwrite( &reg, sizeof(REGSTAT), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	}

	// RAM STATE
	{
	RAMSTAT	ram;
	DWORD	size = 0;

	::ZeroMemory( &hdr, sizeof(BLOCKHDR) );
	::ZeroMemory( &ram, sizeof(RAMSTAT) );

	// SAVE RAM STATE
	::memcpy( ram.RAM, RAM, sizeof(ram.RAM) );
	::memcpy( ram.BGPAL, BGPAL, sizeof(ram.BGPAL) );
	::memcpy( ram.SPPAL, SPPAL, sizeof(ram.SPPAL) );
	::memcpy( ram.SPRAM, SPRAM, sizeof(ram.SPRAM) );

	// S-RAM STATE(使用/未使用に関わらず存在すればセ?ブする)
	if( rom->IsSAVERAM() ) {
		size = SAVERAM_SIZE;
	}

	// Create Header
	::memcpy( hdr.ID, "RAM DATA", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0100;
	hdr.BlockSize    = size+sizeof(RAMSTAT);

	// Write File
	if( ::fwrite( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	if( ::fwrite( &ram, sizeof(RAMSTAT), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	if( rom->IsSAVERAM() ) {
		if( ::fwrite( WRAM, SAVERAM_SIZE, 1, fp ) != 1 )
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}
	}

	// BANK STATE
	{
	MMUSTAT mmu;
	DWORD	size;

	::ZeroMemory( &hdr, sizeof(BLOCKHDR) );
	::ZeroMemory( &mmu, sizeof(MMUSTAT) );

	size = 0;
	// SAVE CPU MEMORY BANK DATA
	// BANK0,1,2はバンクセ?ブに関係なし
	// VirtuaNES0.30から
	// バンク３はSRAM使用に関わらずセ?ブ
	for( i = 3; i < 8; i++ ) {
		mmu.CPU_MEM_TYPE[i] = CPU_MEM_TYPE[i];
		mmu.CPU_MEM_PAGE[i] = CPU_MEM_PAGE[i];

		if( CPU_MEM_TYPE[i] == BANKTYPE_RAM
		 || CPU_MEM_TYPE[i] == BANKTYPE_DRAM ) {
			size += 8*1024;	// 8K BANK
		}
	}

	// SAVE VRAM MEMORY DATA
	for( i = 0; i < 12; i++ ) {
		mmu.PPU_MEM_TYPE[i] = PPU_MEM_TYPE[i];
		mmu.PPU_MEM_PAGE[i] = PPU_MEM_PAGE[i];
	}
	size += 4*1024;	// 1K BANK x 4 (VRAM)

	for( i = 0; i < 8; i++ ) {
		mmu.CRAM_USED[i] = CRAM_USED[i];
		if( CRAM_USED[i] != 0 ) {
			size += 4*1024;	// 4K BANK
		}
	}

	// Create Header
	::memcpy( hdr.ID, "MMU DATA", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0200;
	hdr.BlockSize    = size+sizeof(MMUSTAT);

	// Write File
	if( ::fwrite( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	if( ::fwrite( &mmu, sizeof(MMUSTAT), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}

	// WRITE CPU RAM MEMORY BANK
	for( i = 3; i < 8; i++ ) {
		if( mmu.CPU_MEM_TYPE[i] != BANKTYPE_ROM ) {
			if( ::fwrite( CPU_MEM_BANK[i], 8*1024, 1, fp ) != 1 ) {
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}
	}
	// WRITE VRAM MEMORY(常に4K分すべて書き込む)
	if( ::fwrite( VRAM, 4*1024, 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}

	// WRITE CRAM MEMORY
	for( i = 0; i < 8; i++ ) {
		if( CRAM_USED[i] != 0 ) {
			if( ::fwrite( &CRAM[0x1000*i], 4*1024, 1, fp ) != 1 ) {
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}
	}
	}

	// MMC STATE
	{
	MMCSTAT	mmc;

	::ZeroMemory( &hdr, sizeof(BLOCKHDR) );
	::ZeroMemory( &mmc, sizeof(MMCSTAT) );

	// Create Header
	::memcpy( hdr.ID, "MMC DATA", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0100;
	hdr.BlockSize    = sizeof(MMCSTAT);

	if( mapper->IsStateSave() ) {
		mapper->SaveState( mmc.mmcdata );
		// Write File
		if( ::fwrite( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}
		if( ::fwrite( &mmc, sizeof(MMCSTAT), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}
	}
	}

	// CONTROLLER STATE
	{
	}

	// DISKIMAGE STATE
	if( rom->GetMapperNo() == 20 )
	{
	DISKDATA dsk;
	LPBYTE	lpDisk  = rom->GetPROM();
	LPBYTE	lpWrite = rom->GetDISK();
	LONG	DiskSize = 16+65500*rom->GetDiskNo();
	DWORD	data;

	::ZeroMemory( &hdr, sizeof(BLOCKHDR) );
	::ZeroMemory( &dsk, sizeof(DISKDATA) );

	// 相違数をカウント
	for( i = 16; i < DiskSize; i++ ) {
		if( lpWrite[i] )
			dsk.DifferentSize++;
	}

	::memcpy( hdr.ID, "DISKDATA", sizeof(hdr.ID) );
	hdr.BlockVersion = 0x0210;
	hdr.BlockSize    = 0;

	// Write File
	if( ::fwrite( &hdr, sizeof(BLOCKHDR), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}
	// Write File
	if( ::fwrite( &dsk, sizeof(DISKDATA), 1, fp ) != 1 ) {
		// フ?イルの書き込みに失敗しました
		throw	CApp::GetErrorString( IDS_ERROR_WRITE );
	}

	for( i = 16; i < DiskSize; i++ ) {
		if( lpWrite[i] ) {
			data = i & 0x00FFFFFF;
			data |= ((DWORD)lpDisk[i]&0xFF)<<24;

			// Write File
			if( ::fwrite( &data, sizeof(DWORD), 1, fp ) != 1 ) {
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}
	}
	}
}

INT	NES::GetDiskNo()
{
	return	rom->GetDiskNo();
}

void	NES::SoundSetup()
{
	apu->SoundSetup();
}

void	NES::Command( NESCOMMAND cmd )
{
	CommandParam( cmd, 0 );
}

BOOL	NES::CommandParam( NESCOMMAND cmd, INT param )
{
	switch( cmd ) {
		case	NESCMD_NONE:
			break;
		case	NESCMD_DISK_THROTTLE_ON:
			if( Config.emulator.bDiskThrottle ) {
				m_bDiskThrottle = TRUE;
			}
			break;
		case	NESCMD_DISK_THROTTLE_OFF:
			m_bDiskThrottle = FALSE;
			break;
		case	NESCMD_DISK_EJECT:
			mapper->ExCmdWrite( Mapper::EXCMDWR_DISKEJECT, 0 );
			m_CommandRequest = (INT)cmd;
			break;
		case	NESCMD_DISK_0A:
			if( rom->GetDiskNo() > 0 ) {
				mapper->ExCmdWrite( Mapper::EXCMDWR_DISKINSERT, 0 );
				m_CommandRequest = (INT)cmd;
			}
			break;
		case	NESCMD_DISK_0B:
			if( rom->GetDiskNo() > 1 ) {
				mapper->ExCmdWrite( Mapper::EXCMDWR_DISKINSERT, 1 );
				m_CommandRequest = (INT)cmd;
			}
			break;
		case	NESCMD_DISK_1A:
			if( rom->GetDiskNo() > 2 ) {
				mapper->ExCmdWrite( Mapper::EXCMDWR_DISKINSERT, 2 );
				m_CommandRequest = (INT)cmd;
			}
			break;
		case	NESCMD_DISK_1B:
			if( rom->GetDiskNo() > 3 ) {
				mapper->ExCmdWrite( Mapper::EXCMDWR_DISKINSERT, 3 );
				m_CommandRequest = (INT)cmd;
			}
			break;

		case	NESCMD_HWRESET:
			Reset();
			m_CommandRequest = (INT)cmd;
			break;
		case	NESCMD_SWRESET:
			SoftReset();
			m_CommandRequest = (INT)cmd;
			break;

		case	NESCMD_EXCONTROLLER:
			pad->SetExController( param&0xFF );
			m_CommandRequest = 0x0100|(param&0xFF);
			break;

		case	NESCMD_SOUND_MUTE:
			return	apu->SetChannelMute( (BOOL)param ); // リ??ン値は変更後の?ュ?ト状態
	}

	return	TRUE;
}

BOOL	NES::Snapshot()
{
FILE*	fp = NULL;

	try {
#if	1
		SYSTEMTIME	now;
		::GetLocalTime( &now );

		CHAR	name[_MAX_PATH];
		sprintf( name, "%s %04d%02d%02d%02d%02d%02d%01d.bmp", rom->GetRomName(),
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds/100 );
#else
		CHAR	name[_MAX_PATH];
		sprintf( name, "%s%04d.bmp", rom->GetRomName(), m_nSnapNo );
		if( ++m_nSnapNo > 9999 )
			m_nSnapNo = 9999;
#endif
		string	tempstr;
		if( Config.path.bSnapshotPath ) {
			CreateDirectory( Config.path.szSnapshotPath, NULL );
			tempstr = CPathlib::MakePath( Config.path.szSnapshotPath, name );
		} else {
			tempstr = CPathlib::MakePath( rom->GetRomPath(), name );
		}
		DEBUGOUT( "Snapshot: %s\n", tempstr.c_str() );

		if( !(fp = ::fopen( tempstr.c_str(), "wb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, tempstr.c_str() );
			throw	szErrorString;
		}

		LPBYTE	lpScn = ppu->GetScreenPtr();

		BITMAPFILEHEADER bfh;
		BITMAPINFOHEADER bih;
		RGBQUAD		 rgb[256];

		::ZeroMemory( &bfh, sizeof(bfh) );
		::ZeroMemory( &bih, sizeof(bih) );
		::ZeroMemory( rgb, sizeof(rgb) );

		bfh.bfType = 0x4D42;	// 'BM'
		bfh.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*256;
		bfh.bfSize = bfh.bfOffBits+256*240;

		bih.biSize          = sizeof(bih);
		bih.biWidth         = 256;
		bih.biHeight        = 240;
		bih.biPlanes        = 1;
		bih.biBitCount      = 8;
		bih.biCompression   = BI_RGB;
		bih.biSizeImage     = 0;
		bih.biXPelsPerMeter = 0;
		bih.biYPelsPerMeter = 0;
		bih.biClrUsed       = 256;
		bih.biClrImportant  = 0;

		DirectDraw.GetPaletteData( rgb );

		if( ::fwrite( &bfh, sizeof(bfh), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}
		if( ::fwrite( &bih, sizeof(bih), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}
		if( ::fwrite( &rgb, sizeof(rgb), 1, fp ) != 1 ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}

		lpScn += 8;
		for( INT i = 239; i >= 0; i-- ) {
			if( ::fwrite( &lpScn[(256+16)*i], 256, 1, fp ) != 1 ) {
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}

		FCLOSE( fp );
	} catch( CHAR* str ) {
		DEBUGOUT( "Snapshot error.\n" );
		FCLOSE( fp );
		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "Snapshot error.\n" );
		FCLOSE( fp );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	TRUE;
}

BOOL	NES::IsMovieFile( const char* fname )
{
FILE*	fp = NULL;
MOVIEFILEHDR	header;

	if( !(fp = ::fopen( fname, "rb" )) )
		return	FALSE;

	if( ::fread( &header, sizeof(header), 1, fp ) != 1 ) {
		FCLOSE( fp );
		return	FALSE;
	}

	if( ::memcmp( header.ID, "VirtuaNES MV", sizeof(header.ID) ) == 0 ) {
		if( header.BlockVersion == 0x0100 ) {
			return	TRUE;
		} else 
		if( header.BlockVersion == 0x0200 ) {
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL	NES::MoviePlay( const char* fname )
{
	if( rom->IsNSF() )
		return	FALSE;

	if( IsMoviePlay() || IsMovieRec() ) {
		MovieStop();
	}

DEBUGOUT( "NES::MoviePlay\n" );

	try {
		if( !(m_fpMovie = ::fopen( fname, "rb+" )) ) {
			DEBUGOUT( "Movie play error. File not found.\n" );
			// フ?イル無いです
			return	FALSE;
		}

		// 読み込み
		if( ::fread( &m_hedMovie, sizeof(m_hedMovie), 1, m_fpMovie ) != 1 ) {
			// フ?イルの読み込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_READ );
		}

		if( ::memcmp( m_hedMovie.ID, "VirtuaNES MV", sizeof(m_hedMovie.ID) ) == 0 ) {
			m_MovieVersion = m_hedMovie.BlockVersion;

			if( m_MovieVersion == 0x0100 ) {
				if( ::fseek( m_fpMovie, 0, SEEK_SET ) ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
				// ヘッ?再読み込み
				if( ::fread( &m_hedMovieOld, sizeof(m_hedMovieOld), 1, m_fpMovie ) != 1 ) {
					// フ?イルの読み込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_READ );
				}
			} else 
			if( m_MovieVersion == 0x0200 ) {
				if( Config.emulator.bCrcCheck ) {
					if( rom->GetMapperNo() != 20 ) {
					// FDS以外
						if( m_hedMovie.Ext0 != rom->GetPROM_CRC() ) {
							FCLOSE( m_fpMovie );
							return	FALSE;	// 違うじゃん
						}
					} else {
					// FDS
						if( m_hedMovie.Ext0 !=       rom->GetGameID()
						 || m_hedMovie.Ext1 != (WORD)rom->GetMakerID()
						 || m_hedMovie.Ext2 != (WORD)rom->GetDiskNo() ) {
							FCLOSE( m_fpMovie );
							return	FALSE;	// 違うじゃん
						}
					}
				}
				if( m_hedMovie.CRC != 0 ) {
					if( CRC::Crc( sizeof(m_hedMovie)-sizeof(DWORD), (LPBYTE)&m_hedMovie ) != m_hedMovie.CRC ) {
						FCLOSE( m_fpMovie );
						return	FALSE;	// 違うじゃん
					}
				}
				// おっけ?
			} else {
				FCLOSE( m_fpMovie );
				return	FALSE;
			}
		}

		// ステ?ト読み込み
		ReadState( m_fpMovie );

		LONG	MovieOffset;

		if( m_MovieVersion == 0x0100 ) {
			m_MovieStepTotal = m_hedMovieOld.MovieStep;
			MovieOffset = m_hedMovieOld.MovieOffset;
		} else 
		if( m_MovieVersion == 0x0200 ) {
			m_MovieControl = m_hedMovie.Control;
			m_MovieStepTotal = m_hedMovie.MovieStep;
			MovieOffset = m_hedMovie.MovieOffset;
		}
		if( ::fseek( m_fpMovie, MovieOffset, SEEK_SET ) ) {
			// フ?イルの読み込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_READ );
		}

		// ??ビ?が記?されていない？
		if( m_MovieStepTotal == 0 ) {
			MovieStop();
			return	FALSE;
		}

		m_bMoviePlay = TRUE;
		m_MovieStep = 0;
	} catch( CHAR* str ) {
		DEBUGOUT( "Movie play error. %s\n", str );
		FCLOSE( m_fpMovie );
		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "Movie play error.\n" );
		FCLOSE( m_fpMovie );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	TRUE;
}

BOOL	NES::MovieRec( const char* fname )
{
	if( rom->IsNSF() )
		return	FALSE;

	if( IsMoviePlay() || IsMovieRec() ) {
		MovieStop();
	}

DEBUGOUT( "NES::MovieRec\n" );

	try {
		if( !(m_fpMovie = ::fopen( fname, "wb" )) ) {
			// xxx フ?イルを開けません
			LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
			sprintf( szErrorString, szErrStr, fname );
			throw	szErrorString;
		}

		::ZeroMemory( &m_hedMovie, sizeof(m_hedMovie) );
		::memcpy( m_hedMovie.ID, "VirtuaNES MV", sizeof(m_hedMovie.ID) );
		m_hedMovie.BlockVersion = 0x0200;
		m_hedMovie.StateStOffset = sizeof(m_hedMovie);

		m_hedMovie.Control |= Config.movie.bUsePlayer[0]?0x01:0x00;
		m_hedMovie.Control |= Config.movie.bUsePlayer[1]?0x02:0x00;
		m_hedMovie.Control |= Config.movie.bUsePlayer[2]?0x04:0x00;
		m_hedMovie.Control |= Config.movie.bUsePlayer[3]?0x08:0x00;
		m_hedMovie.Control |= Config.movie.bRerecord?0x80:0x00;
		m_MovieControl = m_hedMovie.Control;

		// CRC,ID値を書き込む(誤動作防?用)
		if( rom->GetMapperNo() != 20 ) {
		// FDS以外
			m_hedMovie.Ext0 = rom->GetPROM_CRC();
		} else {
		// FDS
			m_hedMovie.Ext0 =       rom->GetGameID();
			m_hedMovie.Ext1 = (WORD)rom->GetMakerID();
			m_hedMovie.Ext2 = (WORD)rom->GetDiskNo();
		}

		// ???書き込み
		if( ::fwrite( &m_hedMovie, sizeof(m_hedMovie), 1, m_fpMovie ) != 1 ) {
			FCLOSE( m_fpMovie );
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}

		// ステ?ト書き込み
		WriteState( m_fpMovie );

		m_hedMovie.MovieOffset = ::ftell( m_fpMovie );
		m_bMovieRec = TRUE;
		m_MovieStep = m_MovieStepTotal = 0;
		m_MovieVersion = 0x0200;
	} catch( CHAR* str ) {
		DEBUGOUT( "Movie record error. %s\n", str );
		FCLOSE( m_fpMovie );
		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "Movie record error.\n" );
		FCLOSE( m_fpMovie );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	TRUE;
}

BOOL	NES::MovieRecAppend( const char* fname )
{
	if( rom->IsNSF() )
		return	FALSE;

	// 記?中は意味が無いぞ
	if( IsMovieRec() )
		return	FALSE;

	if( IsMoviePlay() ) {
		MovieStop();
	}

DEBUGOUT( "NES::MovieAppendRec\n" );

	try {
		if( !(m_fpMovie = ::fopen( fname, "rb" )) ) {
			// フ?イルが無いとき
			if( !(m_fpMovie = ::fopen( fname, "wb" )) ) {
				// xxx フ?イルを開けません
				LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
				sprintf( szErrorString, szErrStr, fname );
				throw	szErrorString;
			}

			::ZeroMemory( &m_hedMovie, sizeof(m_hedMovie) );
			::memcpy( m_hedMovie.ID, "VirtuaNES MV", sizeof(m_hedMovie.ID) );
			m_hedMovie.BlockVersion = 0x0200;
			m_hedMovie.StateStOffset = sizeof(m_hedMovie);

			m_hedMovie.Control |= Config.movie.bUsePlayer[0]?0x01:0x00;
			m_hedMovie.Control |= Config.movie.bUsePlayer[1]?0x02:0x00;
			m_hedMovie.Control |= Config.movie.bUsePlayer[2]?0x04:0x00;
			m_hedMovie.Control |= Config.movie.bUsePlayer[3]?0x08:0x00;
			m_hedMovie.Control |= Config.movie.bRerecord?0x80:0x00;
			m_MovieControl = m_hedMovie.Control;

			// ???書き込み
			if( ::fwrite( &m_hedMovie, sizeof(m_hedMovie), 1, m_fpMovie ) != 1 ) {
				FCLOSE( m_fpMovie );
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
			WriteState( m_fpMovie );
			m_hedMovie.MovieOffset = ::ftell( m_fpMovie );
			m_MovieStep = m_MovieStepTotal = 0;
			m_MovieVersion = 0x0200;
		} else {
			if( !(m_fpMovie = ::fopen( fname, "rb+" )) ) {
				// xxx フ?イルを開けません
				LPCSTR	szErrStr = CApp::GetErrorString( IDS_ERROR_OPEN );
				sprintf( szErrorString, szErrStr, fname );
				throw	szErrorString;
			}
			// 読み込み
			if( ::fseek( m_fpMovie, 0, SEEK_SET ) ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}
			if( ::fread( &m_hedMovie, sizeof(m_hedMovie), 1, m_fpMovie ) != 1 ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}

			if( ::memcmp( m_hedMovie.ID, "VirtuaNES MV", sizeof(m_hedMovie.ID) ) != 0 ) {
				FCLOSE( m_fpMovie );
				return	FALSE;
			}
			// 古いバ?ジョンは捨て
			if( m_hedMovie.BlockVersion != 0x0200 ) {
				FCLOSE( m_fpMovie );
				return	FALSE;
			}

			m_MovieControl = m_hedMovie.Control;
			m_MovieStep = m_MovieStepTotal = m_hedMovie.MovieStep;
			m_MovieVersion = 0x0200;

			if( ::fseek( m_fpMovie, m_hedMovie.StateEdOffset, SEEK_SET ) ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}
//			ReadState( m_fpMovie );
			if( !ReadState( m_fpMovie ) ) {
				FCLOSE( m_fpMovie );
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}
			if( ::fseek( m_fpMovie, m_hedMovie.StateEdOffset, SEEK_SET ) ) {
				// フ?イルの読み込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_READ );
			}
		}
		m_bMovieRec = TRUE;
	} catch( CHAR* str ) {
		DEBUGOUT( "Movie record error. %s\n", str );
		FCLOSE( m_fpMovie );
		throw	str;
#ifndef	_DEBUG
	} catch(...) {
		DEBUGOUT( "Movie record error.\n" );
		FCLOSE( m_fpMovie );
		// 不明なエラ?が発生しました
		throw	CApp::GetErrorString( IDS_ERROR_UNKNOWN );
#endif
	}

	return	TRUE;
}

BOOL	NES::MovieStop()
{
	if( !m_fpMovie && !(m_bMoviePlay||m_bMovieRec) )
		return	FALSE;

DEBUGOUT( "NES::MovieStop\n" );

	DirectDraw.SetMessageString( "Movie stop." );

	if( m_bMovieRec ) {
		m_hedMovie.MovieStep = m_MovieStep;
		m_hedMovie.StateEdOffset = ::ftell( m_fpMovie );

		WriteState( m_fpMovie );
		if( ::fseek( m_fpMovie, 0, SEEK_SET ) ) {
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}

		// CRC 書き込み
		m_hedMovie.CRC = CRC::Crc( sizeof(m_hedMovie)-sizeof(DWORD), (LPBYTE)&m_hedMovie );

		// 最終的なヘッ?書き込み
		if( ::fwrite( &m_hedMovie, sizeof(m_hedMovie), 1, m_fpMovie ) != 1 ) {
			FCLOSE( m_fpMovie );
			// フ?イルの書き込みに失敗しました
			throw	CApp::GetErrorString( IDS_ERROR_WRITE );
		}

		FCLOSE( m_fpMovie );
		m_bMovieRec = FALSE;
	}

	if( m_bMoviePlay ) {
		FCLOSE( m_fpMovie );
		m_bMoviePlay = FALSE;
	}

	return	TRUE;
}

void	NES::GetMovieInfo( WORD& wVersion, DWORD& dwRecordFrames, DWORD& dwRecordTimes )
{
	if( m_hedMovie.BlockVersion < 0x0200 ) {
		wVersion = m_hedMovieOld.BlockVersion;

		dwRecordFrames = m_hedMovieOld.MovieStep;
		dwRecordTimes  = 0xFFFFFFFF;
	} else {
		wVersion = m_hedMovie.BlockVersion;

		dwRecordFrames = m_hedMovie.MovieStep;
		dwRecordTimes  = m_hedMovie.RecordTimes;
	}
}

// 毎フレ??呼び出す
void	NES::Movie()
{
	if( !m_fpMovie && !(m_bMoviePlay||m_bMovieRec) ) {
		m_CommandRequest = 0;	// コレ入れないと?ぬ
		return;
	}

	INT	exctr = pad->GetExController();

	BYTE	Data;
	WORD	wData;
	DWORD	dwData;

	if( m_bMovieRec ) {
		// 最初から拡張ｺﾝﾄﾛｰﾗが設定されていた場合
		if( m_MovieStep == 0 ) {
			if( exctr == PAD::EXCONTROLLER_ZAPPER
			 || exctr == PAD::EXCONTROLLER_PADDLE
			 || exctr == PAD::EXCONTROLLER_CRAZYCLIMBER
			 || exctr == PAD::EXCONTROLLER_TOPRIDER
			 || exctr == PAD::EXCONTROLLER_SPACESHADOWGUN ) {
				// コ?ンドID
				Data = 0xF0;
				// 書き込み
				if( ::fwrite( &Data, sizeof(Data), 1, m_fpMovie ) != 1 ) {
					MovieStop();
					// フ?イルの書き込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_WRITE );
				}
				// 種類
				wData = (WORD)(0x0100|(pad->GetExController()&0x0FF));
				// 書き込み
				if( ::fwrite( &wData, sizeof(wData), 1, m_fpMovie ) != 1 ) {
					MovieStop();
					// フ?イルの書き込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_WRITE );
				}
			}
		}

		if( m_CommandRequest ) {
			// コ?ンドID
			Data = 0xF0;
			// 書き込み
			if( ::fwrite( &Data, sizeof(Data), 1, m_fpMovie ) != 1 ) {
				MovieStop();
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
			// コ?ンド
			wData = (WORD)m_CommandRequest;
			// 書き込み
			if( ::fwrite( &wData, sizeof(wData), 1, m_fpMovie ) != 1 ) {
				MovieStop();
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}
		m_CommandRequest = 0;

		// 拡張ｺﾝﾄﾛｰﾗ
		if( exctr == PAD::EXCONTROLLER_ZAPPER
		 || exctr == PAD::EXCONTROLLER_PADDLE
		 || exctr == PAD::EXCONTROLLER_CRAZYCLIMBER
		 || exctr == PAD::EXCONTROLLER_TOPRIDER
		 || exctr == PAD::EXCONTROLLER_SPACESHADOWGUN ) {
			// 拡張コントロ?ラデ??ID
			Data = 0xF3;
			// 書き込み
			if( ::fwrite( &Data, sizeof(Data), 1, m_fpMovie ) != 1 ) {
				MovieStop();
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
			// デ??
			dwData = pad->GetSyncExData();

			// 書き込み
			if( ::fwrite( &dwData, sizeof(dwData), 1, m_fpMovie ) != 1 ) {
				MovieStop();
				// フ?イルの書き込みに失敗しました
				throw	CApp::GetErrorString( IDS_ERROR_WRITE );
			}
		}

		dwData = pad->GetSyncData();
		for( INT i = 0; i < 4; i++ ) {
			Data = (BYTE)(dwData>>(i*8));
			if( m_MovieControl & (1<<i) ) {
				// 書き込み
				if( ::fwrite( &Data, sizeof(Data), 1, m_fpMovie ) != 1 ) {
					MovieStop();
					// フ?イルの書き込みに失敗しました
					throw	CApp::GetErrorString( IDS_ERROR_WRITE );
				}
			}
		}

		m_MovieStep++;
	}

	if( m_bMoviePlay ) {
		if( m_MovieVersion == 0x0100 ) {
		// Ver0.24まで
			DWORD	data;

			// ??ビ?再生終了？
			if( m_MovieStep >= m_MovieStepTotal ) {
				if( !Config.movie.bLoopPlay ) {
					MovieStop();
					return;
				} else {
					// 一旦再生中じゃないって事にする
					m_bMoviePlay = FALSE;
					m_MovieStep = 0;
					::fseek( m_fpMovie, m_hedMovieOld.StateStOffset, SEEK_SET );
					// ステ?ト読み込み
					ReadState( m_fpMovie );
					::fseek( m_fpMovie, m_hedMovieOld.MovieOffset, SEEK_SET );
					// 再生中って事にする
					m_bMoviePlay = TRUE;
				}
			}

			do {
				// 読み込み
				if( ::fread( &data, sizeof(data), 1, m_fpMovie ) != 1 ) {
					// 終了？
					MovieStop();
					return;
				}
				// コ?ンド？
				if( data & 0x0C0C0000 ) {
					m_CommandRequest = (INT)data&0xFFFF;
					if( m_CommandRequest < 0x0100 ) {
						Command( (NESCOMMAND)m_CommandRequest );
					} else {
						// 拡張ｺﾝﾄﾛｰﾗ
						CommandParam( NESCMD_EXCONTROLLER, m_CommandRequest & 0x00FF );
					}
				}
			} while( data&0x0C0C0000 );

			pad->SetSyncData( data );

			// 拡張ｺﾝﾄﾛｰﾗ
			if( exctr == PAD::EXCONTROLLER_ZAPPER
			 || exctr == PAD::EXCONTROLLER_PADDLE
			 || exctr == PAD::EXCONTROLLER_CRAZYCLIMBER
			 || exctr == PAD::EXCONTROLLER_TOPRIDER
			 || exctr == PAD::EXCONTROLLER_SPACESHADOWGUN ) {
				// 読み込み
				if( ::fread( &data, sizeof(data), 1, m_fpMovie ) != 1 ) {
					// 終了？
					MovieStop();
					return;
				}
				pad->SetSyncExData( data );
			}

			// カウン?増やす
			m_MovieStep++;
		} else 
		if( m_MovieVersion == 0x0200 ) {
		// Ver0.30以?
			DWORD	dwPadData = 0;
			INT	num = 0;
			BYTE	PadBuf[4];

			::ZeroMemory( PadBuf, sizeof(PadBuf) );

			// ??ビ?再生終了？
			if( m_MovieStep >= m_MovieStepTotal ) {
				if( !Config.movie.bLoopPlay ) {
					MovieStop();
					return;
				} else {
					// 一旦再生中じゃないって事にする
					m_bMoviePlay = FALSE;
					m_MovieStep = 0;
					::fseek( m_fpMovie, m_hedMovie.StateStOffset, SEEK_SET );
					// ステ?ト読み込み
					ReadState( m_fpMovie );
					::fseek( m_fpMovie, m_hedMovie.MovieOffset, SEEK_SET );
					// 再生中って事にする
					m_bMoviePlay = TRUE;
				}
			}

			do {
				// 読み込み
				if( ::fread( &Data, sizeof(Data), 1, m_fpMovie ) != 1 ) {
					// 終了？
					MovieStop();
					return;
				}

				// コ?ンド？
				if( (Data & 0xF0) == 0xF0 ) {
					if( Data == 0xF0 ) {
						// 読み込み
						if( ::fread( &wData, sizeof(wData), 1, m_fpMovie ) != 1 ) {
							// 終了？
							MovieStop();
							return;
						}
						if( wData < 0x0100 ) {
							Command( (NESCOMMAND)((INT)wData) );
						} else {
							// 拡張ｺﾝﾄﾛｰﾗ
							CommandParam( NESCMD_EXCONTROLLER, ((INT)wData) & 0x00FF );
						}
					} else 
					if( Data == 0xF3 ) {
						// 読み込み
						if( ::fread( &dwData, sizeof(dwData), 1, m_fpMovie ) != 1 ) {
							// 終了？
							MovieStop();
							return;
						}
						pad->SetSyncExData( dwData );
					} else {
						// デ??ぶっ壊れてる？終了じゃ
						MovieStop();
						return;
					}
				} else {
					// 未使用プレイヤ?分すっ飛ばし
					while( !(m_MovieControl & (1<<num)) && (num < 4) ) {
						PadBuf[num] = 0;
						num++;
					}
					PadBuf[num] = Data;
					num++;
					// 未使用プレイヤ?分すっ飛ばし
					while( !(m_MovieControl & (1<<num)) && (num < 4) ) {
						PadBuf[num] = 0;
						num++;
					}
				}
			} while( num < 4 );

			dwData = (((DWORD)PadBuf[3])<<24)|(((DWORD)PadBuf[2])<<16)|(((DWORD)PadBuf[1])<<8)|((DWORD)PadBuf[0]);
			pad->SetSyncData( dwData );

			// カウン?増やす
			m_MovieStep++;
		}
	}

	m_CommandRequest = 0;
}

// For Cheat
void	NES::CheatInitial()
{
	m_CheatCode.clear();
}

BOOL	NES::IsCheatCodeAdd()
{
	BOOL bRet = m_bCheatCodeAdd;
	m_bCheatCodeAdd = FALSE;

	return	bRet;
}

INT	NES::GetCheatCodeNum()
{
	return	m_CheatCode.size();
}

BOOL	NES::GetCheatCode( INT no, CHEATCODE& code )
{
	if( m_CheatCode.size()-1 < no )
		return	FALSE;

	code = m_CheatCode[no];
	return	TRUE;
}

void	NES::SetCheatCodeFlag( INT no, BOOL bEnable )
{
	if( m_CheatCode.size()-1 < no )
		return;

	if( bEnable ) {
		m_CheatCode[no].enable |= CHEAT_ENABLE;
	} else {
		m_CheatCode[no].enable &= ~CHEAT_ENABLE;
	}
}

void	NES::SetCheatCodeAllFlag( BOOL bEnable, BOOL bKey )
{
	for( INT i = 0; i < m_CheatCode.size(); i++ ) {
		if( !bKey ) {
			if( bEnable ) {
				m_CheatCode[i].enable |= CHEAT_ENABLE;
			} else {
				m_CheatCode[i].enable &= ~CHEAT_ENABLE;
			}
		} else if( !(m_CheatCode[i].enable&CHEAT_KEYDISABLE) ) {
			if( bEnable ) {
				m_CheatCode[i].enable |= CHEAT_ENABLE;
			} else {
				m_CheatCode[i].enable &= ~CHEAT_ENABLE;
			}
		}
	}
}

void	NES::ReplaceCheatCode( INT no, CHEATCODE code )
{
	if( m_CheatCode.size()-1 < no )
		return;

	m_CheatCode[no] = code;
}

void	NES::AddCheatCode( CHEATCODE code )
{
	m_CheatCode.push_back( code );
	m_bCheatCodeAdd = TRUE;
}

void	NES::DelCheatCode( INT no )
{
	if( m_CheatCode.size()-1 < no )
		return;

	//m_CheatCode.erase( &m_CheatCode[no] );
}

DWORD	NES::CheatRead( INT length, WORD addr )
{
	DWORD	data = 0;
	for( INT i = 0; i <= length; i++ ) {
		data |= (DWORD)Read( addr+i )*(1<<(i*8));
	}

	return	data;
}

void	NES::CheatWrite( INT length, WORD addr, DWORD data )
{
	for( INT i = 0; i <= length; i++ ) {
		Write( (WORD)(addr+i), data&0xFF );
		data >>= 8;
	}
}

void	NES::CheatCodeProcess()
{
	for( vector<CHEATCODE>::iterator it = m_CheatCode.begin(); it != m_CheatCode.end(); it++ ) {
		if( !(it->enable & CHEAT_ENABLE) )
			continue;

		switch( it->type ) {
			case	CHEAT_TYPE_ALWAYS:
				CheatWrite( it->length, it->address, it->data );
				break;
			case	CHEAT_TYPE_ONCE:
				CheatWrite( it->length, it->address, it->data );
				it->enable = 0;
				break;
			case	CHEAT_TYPE_GREATER:
				if( CheatRead( it->length, it->address ) > it->data ) {
					CheatWrite( it->length, it->address, it->data );
				}
				break;
			case	CHEAT_TYPE_LESS:
				if( CheatRead( it->length, it->address ) < it->data ) {
					CheatWrite( it->length, it->address, it->data );
				}
				break;
		}
	}
}

void	NES::GenieInitial()
{
	m_bCheatCodeAdd = FALSE;
	m_GenieCode.clear();
}

void	NES::GenieLoad( char* fname )
{
FILE*	fp = NULL;

	CHAR	buf[256];
	GENIECODE	code;
	BYTE	codetmp[9];
	INT	no;

	if( (fp = ::fopen( fname, "r" )) ) {
		m_GenieCode.clear();

		while( ::fgets( buf, sizeof(buf), fp ) ) {
			if( buf[0] == ';' )
				continue;
			if( buf[0] == 0x0D || buf[0] == 0x0A )
				continue;

			if( ::strlen( buf ) < 6 )
				continue;

			code.address = 0;
			code.data = 0;
			code.cmp = 0;

			for( no = 0; isalpha(buf[no]) && no < 8; no++ ) {
				switch( buf[no] ) {
					case	'A': codetmp[no] = 0x00; break;
					case	'P': codetmp[no] = 0x01; break;
					case	'Z': codetmp[no] = 0x02; break;
					case	'L': codetmp[no] = 0x03; break;
					case	'G': codetmp[no] = 0x04; break;
					case	'I': codetmp[no] = 0x05; break;
					case	'T': codetmp[no] = 0x06; break;
					case	'Y': codetmp[no] = 0x07; break;
					case	'E': codetmp[no] = 0x08; break;
					case	'O': codetmp[no] = 0x09; break;
					case	'X': codetmp[no] = 0x0A; break;
					case	'U': codetmp[no] = 0x0B; break;
					case	'K': codetmp[no] = 0x0C; break;
					case	'S': codetmp[no] = 0x0D; break;
					case	'V': codetmp[no] = 0x0E; break;
					case	'N': codetmp[no] = 0x0F; break;
				}
			}

			if( no == 6 ) {
				// Address
				code.address |= (WORD)(codetmp[3] & 0x07)<<12;
				code.address |= (WORD)(codetmp[4] & 0x08)<< 8;
				code.address |= (WORD)(codetmp[5] & 0x07)<< 8;
				code.address |= (WORD)(codetmp[1] & 0x08)<< 4;
				code.address |= (WORD)(codetmp[2] & 0x07)<< 4;
				code.address |= (WORD)(codetmp[3] & 0x08);
				code.address |= (WORD)(codetmp[4] & 0x07);
				// Data
				code.data |= (codetmp[0] & 0x08)<<4;
				code.data |= (codetmp[1] & 0x07)<<4;
				code.data |= (codetmp[5] & 0x08);
				code.data |= (codetmp[0] & 0x07);

				m_GenieCode.push_back( code );
			} else
			if( no == 8 ) {
				// Address
				code.address |= 0x8000;
				code.address |= (WORD)(codetmp[3] & 0x07)<<12;
				code.address |= (WORD)(codetmp[4] & 0x08)<< 8;
				code.address |= (WORD)(codetmp[5] & 0x07)<< 8;
				code.address |= (WORD)(codetmp[1] & 0x08)<< 4;
				code.address |= (WORD)(codetmp[2] & 0x07)<< 4;
				code.address |= (WORD)(codetmp[3] & 0x08);
				code.address |= (WORD)(codetmp[4] & 0x07);
				// Data
				code.data |= (codetmp[0] & 0x08)<<4;
				code.data |= (codetmp[1] & 0x07)<<4;
				code.data |= (codetmp[7] & 0x08);
				code.data |= (codetmp[0] & 0x07);
				// Data
				code.cmp  |= (codetmp[6] & 0x08)<<4;
				code.cmp  |= (codetmp[7] & 0x07)<<4;
				code.cmp  |= (codetmp[5] & 0x08);
				code.cmp  |= (codetmp[6] & 0x07);

				m_GenieCode.push_back( code );
			}
		}

		GenieCodeProcess();
	}

	FCLOSE( fp );
}

void	NES::GenieCodeProcess()
{
	WORD	addr;

	for( INT i = 0; i < m_GenieCode.size(); i++ ) {
		addr = m_GenieCode[i].address;
		if( addr & 0x8000 ) {
		// 8character codes
			if( CPU_MEM_BANK[addr>>13][addr&0x1FFF] == m_GenieCode[i].cmp ) {
				CPU_MEM_BANK[addr>>13][addr&0x1FFF] = m_GenieCode[i].data;
			}
		} else {
		// 6character codes
			addr |= 0x8000;
			CPU_MEM_BANK[addr>>13][addr&0x1FFF] = m_GenieCode[i].data;
		}
	}
}

