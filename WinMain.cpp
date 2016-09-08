#define	INITGUID
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <crtdbg.h>

#include <string>
using namespace std;

#include "DebugOut.h"
#include "VirtuaNESres.h"

#include "App.h"
#include "Registry.h"
#include "Pathlib.h"
#include "MMTimer.h"

#include "Wnd.h"
#include "WndHook.h"
#include "MainFrame.h"
#include "Plugin.h"
#include "Config.h"
#include "Recent.h"

#include "DirectDraw.h"
#include "DirectSound.h"
#include "DirectInput.h"

INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
#if	_DEBUG
	// ���������[�N���o
	int	Flag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	Flag |= _CRTDBG_LEAK_CHECK_DF;
	Flag &= ~_CRTDBG_CHECK_ALWAYS_DF;
	_CrtSetDbgFlag( Flag );
#endif
	// ���C���t���[���E�C���h�E�I�u�W�F�N�g
	CMainFrame	MainFrame;

	// Mutex
	HANDLE	hMutex = NULL;

	// �A�v���P�[�V�����C���X�^���X���̐ݒ�
	CHAR	szPath[ _MAX_PATH ];
	GetModuleFileName( hInstance, szPath, sizeof(szPath) );
	string	ModulePath = CPathlib::SplitPath( szPath );
	CApp::SetModulePath( ModulePath.c_str() );
	DEBUGOUT( "Module Path:\"%s\"\n", ModulePath.c_str() );

	CApp::SetInstance( hInstance );
	CApp::SetPrevInstance( hPrevInstance );
	CApp::SetCmdLine( lpCmdLine );
	CApp::SetCmdShow( nCmdShow );

//	CRegistry::SetRegistryKey( "Emulators\\VirtuaNES" );
	CRegistry::SetRegistryKey( "VirtuaNES.ini" );

	if( !CPlugin::FindPlugin( CApp::GetModulePath() ) ) {
		::MessageBox( NULL, "Language plug-in is not found.", "VirtuaNES", MB_ICONERROR|MB_OK );
		goto	_Error_Exit;
	}
	DEBUGOUT( "Plugin Path:\"%s\"\n", CPlugin::GetPluginPath() );
	DEBUGOUT( "Language   :\"%s\"\n", CPlugin::GetPluginLanguage() );
	DEBUGOUT( "LCID       :\"%d\" \"0x%04X\"\n", CPlugin::GetPluginLocaleID(), CPlugin::GetPluginLocaleID() );

	HINSTANCE hPlugin;
	if( !(hPlugin = CPlugin::LoadPlugin()) ) {
		::MessageBox( NULL, "Language plug-in load failed.", "VirtuaNES", MB_ICONERROR|MB_OK );
		goto	_Error_Exit;
	}
	CApp::SetPlugin( hPlugin );

	::InitCommonControls();

	// �ݒ�̃��[�h
	Config.Load();
	CRecent::Load();

	// ��d�N���̖h�~
	hMutex = ::CreateMutex( NULL, FALSE, VIRTUANES_MUTEX );
	if( ::GetLastError() == ERROR_ALREADY_EXISTS ) {
		::CloseHandle( hMutex );
		if( Config.general.bDoubleExecute ) {
			HWND	hWnd = ::FindWindow( VIRTUANES_WNDCLASS, NULL );

			// �N�����Ă��������t�H�A�O���E���h�ɂ���
			::SetForegroundWindow( hWnd );

			// �R�}���h���C������������Ȃ瓮�쒆��VirtuaNES�̃E�C���h�E�Ƀt�@�C����
			// ���b�Z�[�W�𑗂���Ă�����œ��삳����
			// (���R�̗l�ɑΉ��o�[�W�����łȂ��ƃ_��)
			if( ::strlen( lpCmdLine ) > 0 ) {
				LPSTR	pCmd = lpCmdLine;
				if( lpCmdLine[0] == '"' ) {	// Shell execute!!
					lpCmdLine++;
					if( lpCmdLine[::strlen( lpCmdLine )-1] == '"' ) {
						lpCmdLine[::strlen( lpCmdLine )-1] = '\0';
					}
				}

				COPYDATASTRUCT	cds;
				cds.dwData = 0;
				cds.lpData = (void*)lpCmdLine;
				cds.cbData = ::strlen(lpCmdLine)+1; //  �I�[��NULL������
				//  �����񑗐M
				::SendMessage( hWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds );
			}

			// �I��
			goto	_DoubleExecute_Exit;
		}
	}

	if( !MainFrame.Create(NULL) )
		goto	_Error_Exit;
	DEBUGOUT( "CreateWindow ok.\n" );

	// ���C���E�C���h�E�̕\��
	::ShowWindow( CApp::GetHWnd(), CApp::GetCmdShow() );
	::UpdateWindow( CApp::GetHWnd() );

	// �t�b�N
	CWndHook::Initialize();

	// �R�}���h���C��
	if( ::strlen( lpCmdLine ) > 0 ) {
		LPSTR	pCmd = lpCmdLine;
		if( lpCmdLine[0] == '"' ) {	// Shell execute!!
			lpCmdLine++;
			if( lpCmdLine[::strlen( lpCmdLine )-1] == '"' ) {
				lpCmdLine[::strlen( lpCmdLine )-1] = '\0';
			}
		}
	}

	if( ::strlen( lpCmdLine ) > 0 ) {
		::PostMessage( CApp::GetHWnd(), WM_VNS_COMMANDLINE, 0, (LPARAM)lpCmdLine );
	}

	MSG	msg;
	while( ::GetMessage( &msg, NULL, 0, 0 ) ) {
		// ���C���E�C���h�E�̃��b�Z�[�W�t�B���^�����O
		if( CApp::GetHWnd() == msg.hwnd ) {
			CWnd* pWnd = (CWnd*)::GetWindowLong( msg.hwnd, GWL_USERDATA );
			if( pWnd ) {
				if( pWnd->PreTranslateMessage( &msg ) )
					continue;
			}
		}
		if( CWndList::IsDialogMessage( &msg ) )
			continue;
		::TranslateMessage( &msg );
		::DispatchMessage( &msg );
	}
	// �t�b�N
	CWndHook::Release();

	// �ݒ�̕ۑ�
	Config.Save();
	CRecent::Save();

	// DirectX�n�j��
	DirectDraw.ReleaseDDraw();
	DirectSound.ReleaseDSound();
	DirectInput.ReleaseDInput();

	if( hMutex )
		::ReleaseMutex( hMutex );
	CLOSEHANDLE( hMutex );

_DoubleExecute_Exit:
	::FreeLibrary( CApp::GetPlugin() );

	return	msg.wParam;

_Error_Exit:
	// DirectX�n�j��
	DirectDraw.ReleaseDDraw();
	DirectSound.ReleaseDSound();
	DirectInput.ReleaseDInput();

	if( CApp::GetPlugin() ) {
		::FreeLibrary( CApp::GetPlugin() );
	}

	return	-1;
}
