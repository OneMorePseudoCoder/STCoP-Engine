#include "stdafx.h"
#include "MainMenu.h"
#include "UI/UIDialogWnd.h"
#include "ui/UIMessageBoxEx.h"
#include "../xrEngine/xr_IOConsole.h"
#include "../xrEngine/IGame_Level.h"
#include "../xrEngine/CameraManager.h"
#include "xr_Level_controller.h"
#include "ui\UITextureMaster.h"
#include "ui\UIXmlInit.h"
#include <dinput.h>
#include "ui\UIBtnHint.h"
#include "UICursor.h"
#include "string_table.h"
#include "../xrCore/os_clipboard.h"

#include <shellapi.h>

#include "object_broker.h"

#define GAME_VERSION "1.6.02"

extern bool b_shniaganeed_pp;

CMainMenu*	MainMenu()	{return (CMainMenu*)g_pGamePersistent->m_pMainMenu; };
//----------------------------------------------------------------------------------
#define INIT_MSGBOX(_box, _template)	{ _box = xr_new<CUIMessageBoxEx>(); _box->InitMessageBox(_template);}
//----------------------------------------------------------------------------------

CMainMenu::CMainMenu	()
{
	m_Flags.zero					();
	m_startDialog					= NULL;
	m_screenshotFrame				= u32(-1);
	g_pGamePersistent->m_pMainMenu	= this;
	if (Device.b_is_Ready)			OnDeviceCreate();  	
	ReadTextureInfo					();
	CUIXmlInit::InitColorDefs		();
	g_btnHint						= NULL;
	g_statHint						= NULL;
	m_deactivated_frame				= 0;	
	
	//-------------------------------------------

	m_start_time					= 0;

	g_btnHint						= xr_new<CUIButtonHint>();
	g_statHint						= xr_new<CUIButtonHint>();

	Device.seqFrame.Add		(this,REG_PRIORITY_LOW-1000);
}

CMainMenu::~CMainMenu	()
{
	Device.seqFrame.Remove			(this);
	xr_delete						(g_btnHint);
	xr_delete						(g_statHint);
	xr_delete						(m_startDialog);
	g_pGamePersistent->m_pMainMenu	= NULL;
}

void CMainMenu::ReadTextureInfo()
{
	FS_FileSet fset;
	FS.file_list(fset, "$game_config$", FS_ListFiles,"ui\\textures_descr\\*.xml");
	FS_FileSetIt fit	= fset.begin();
	FS_FileSetIt fit_e	= fset.end();

	for( ;fit!=fit_e; ++fit)
	{
    	string_path	fn1, fn2,fn3;
        _splitpath	((*fit).name.c_str(),fn1,fn2,fn3,0);
		xr_strcat(fn3,".xml");

		CUITextureMaster::ParseShTexInfo(fn3);
	}
}

extern ENGINE_API BOOL	bShowPauseString;

void CMainMenu::Activate(bool bActivate)
{
	if (!!m_Flags.test(flActive) == bActivate)		
		return;

	if (m_Flags.test(flGameSaveScreenshot))		
		return;

	if ((m_screenshotFrame == Device.dwFrame) || (m_screenshotFrame == Device.dwFrame-1) || (m_screenshotFrame == Device.dwFrame+1))	
		return;

	if (bActivate)
	{
		b_shniaganeed_pp = true;
		Device.Pause(TRUE, FALSE, TRUE, "mm_activate1");
		m_Flags.set(flActive | flNeedChangeCapture, TRUE);

		m_Flags.set(flRestoreCursor, GetUICursor().IsVisible());

		if (!ReloadUI())				
			return;

		m_Flags.set(flRestoreConsole, Console->bVisible);

		m_Flags.set(flRestorePause,Device.Paused());

		Console->Hide();

		m_Flags.set(flRestorePauseStr, bShowPauseString);
		bShowPauseString = FALSE;
		if (!m_Flags.test(flRestorePause))
			Device.Pause(TRUE, TRUE, FALSE, "mm_activate2");

		if (g_pGameLevel)
		{
			Device.seqFrame.Remove(g_pGameLevel);
			Device.seqRender.Remove(g_pGameLevel);
			CCameraManager::ResetPP();
		};
	
		Device.seqRender.Add(this, 4); // 1-console 2-cursor 3-tutorial

		Console->Execute("stat_memory");
	}
	else
	{
		m_deactivated_frame	= Device.dwFrame;
		m_Flags.set(flActive, FALSE);
		m_Flags.set(flNeedChangeCapture, TRUE);

		Device.seqRender.Remove(this);

		bool b = !!Console->bVisible;
		if (b)
			Console->Hide();

		IR_Release();

		if (b)
			Console->Show();

		if (m_startDialog->IsShown())
			m_startDialog->HideDialog();

		CleanInternals();

		if (g_pGameLevel)
		{
			Device.seqFrame.Add(g_pGameLevel);
			Device.seqRender.Add(g_pGameLevel);
		};

		if (m_Flags.test(flRestoreConsole))
			Console->Show			();

		if (!m_Flags.test(flRestorePause))
			Device.Pause(FALSE, TRUE, FALSE, "mm_deactivate1");

		bShowPauseString = m_Flags.test(flRestorePauseStr);

		if (m_Flags.test(flRestoreCursor))
			GetUICursor().Show();

		Device.Pause(FALSE, TRUE, TRUE, "mm_deactivate2");

		if (m_Flags.test(flNeedVidRestart))
		{
			m_Flags.set(flNeedVidRestart, FALSE);
			Console->Execute("vid_restart");
		}
	}
}

bool CMainMenu::ReloadUI()
{
	if (m_startDialog)
	{
		if (m_startDialog->IsShown())
			m_startDialog->HideDialog();
		CleanInternals();
	}

	DLL_Pure* dlg = NEW_INSTANCE(TEXT2CLSID("MAIN_MNU"));

	if (!dlg) 
	{
		m_Flags.set(flActive | flNeedChangeCapture, FALSE);
		return false;
	}

	xr_delete(m_startDialog);
	m_startDialog = smart_cast<CUIDialogWnd*>(dlg);
	VERIFY(m_startDialog);
	m_startDialog->m_bWorkInPause = true;
	m_startDialog->ShowDialog(true);

	m_activatedScreenRatio = (float)Device.dwWidth / (float)Device.dwHeight > (UI_BASE_WIDTH / UI_BASE_HEIGHT + 0.01f);
	return true;
}

bool CMainMenu::IsActive()
{
	return !!m_Flags.test(flActive);
}

bool CMainMenu::CanSkipSceneRendering()
{
	return IsActive() && !m_Flags.test(flGameSaveScreenshot);
}

static int mouse_button_2_key [] = {MOUSE_1,MOUSE_2,MOUSE_3};
void CMainMenu::IR_OnMousePress(int btn)	
{	
	if (!IsActive())
		return;

	IR_OnKeyboardPress(mouse_button_2_key[btn]);
};

void CMainMenu::IR_OnMouseRelease(int btn)	
{
	if (!IsActive()) 
		return;

	IR_OnKeyboardRelease(mouse_button_2_key[btn]);
};

void CMainMenu::IR_OnMouseHold(int btn)	
{
	if (!IsActive()) 
		return;

	IR_OnKeyboardHold(mouse_button_2_key[btn]);
};

void CMainMenu::IR_OnMouseMove(int x, int y)
{
	if (!IsActive()) 
		return;

	CDialogHolder::IR_UIOnMouseMove(x, y);
};

void CMainMenu::IR_OnMouseStop(int x, int y)
{};

void CMainMenu::IR_OnKeyboardPress(int dik)
{
	if (!IsActive()) 
		return;

	if (is_binded(kCONSOLE, dik))
	{
		Console->Show();
		return;
	}

	if (DIK_F12 == dik)
	{
		Render->Screenshot();
		return;
	}

	CDialogHolder::IR_UIOnKeyboardPress(dik);
};

void CMainMenu::IR_OnKeyboardRelease(int dik)
{
	if (!IsActive()) 
		return;
	
	CDialogHolder::IR_UIOnKeyboardRelease(dik);
};

void CMainMenu::IR_OnKeyboardHold(int dik)	
{
	if (!IsActive()) 
		return;
	
	CDialogHolder::IR_UIOnKeyboardHold(dik);
};

void CMainMenu::IR_OnMouseWheel(int direction)
{
	if (!IsActive()) 
		return;
	
	CDialogHolder::IR_UIOnMouseWheel(direction);
}

bool CMainMenu::OnRenderPPUI_query()
{
	return IsActive() && !m_Flags.test(flGameSaveScreenshot) && b_shniaganeed_pp;
}

extern void draw_wnds_rects();

void CMainMenu::OnRender()
{
	if (m_Flags.test(flGameSaveScreenshot))
		return;

	Render->firstViewPort = MAIN_VIEWPORT;
	Render->lastViewPort = MAIN_VIEWPORT;
	Render->currentViewPort = MAIN_VIEWPORT;
	Render->needPresenting = true ;

	if (g_pGameLevel)
		Render->Calculate();

	Render->Render();
	if (!OnRenderPPUI_query())
	{
		DoRenderDialogs();
		UI().RenderFont();
		draw_wnds_rects();
	}
}

void CMainMenu::OnRenderPPUI_main()
{
	if (!IsActive()) 
		return;

	if (m_Flags.test(flGameSaveScreenshot))
		return;

	UI().pp_start();

	if (OnRenderPPUI_query())
	{
		DoRenderDialogs();
		UI().RenderFont();
	}

	UI().pp_stop();
}

void CMainMenu::OnRenderPPUI_PP()
{
	if (!IsActive()) 
		return;

	if (m_Flags.test(flGameSaveScreenshot))	
		return;

	UI().pp_start();
	
	xr_vector<CUIWindow*>::iterator it = m_pp_draw_wnds.begin();
	for (; it!=m_pp_draw_wnds.end();++it)
	{
		(*it)->Draw();
	}
	UI().pp_stop();
}

//pureFrame
void CMainMenu::OnFrame()
{
	if (m_Flags.test(flNeedChangeCapture))
	{
		m_Flags.set(flNeedChangeCapture, FALSE);
		if (m_Flags.test(flActive))	
			IR_Capture();
		else						
			IR_Release();
	}

	CDialogHolder::OnFrame();

	//screenshot stuff
	if (m_Flags.test(flGameSaveScreenshot) && Device.dwFrame > m_screenshotFrame)
	{
		m_Flags.set(flGameSaveScreenshot,FALSE);
		::Render->Screenshot(IRender_interface::SM_FOR_GAMESAVE, m_screenshot_name);
		
		if (g_pGameLevel && m_Flags.test(flActive))
		{
			Device.seqFrame.Remove(g_pGameLevel);
			Device.seqRender.Remove(g_pGameLevel);
		};

		if (m_Flags.test(flRestoreConsole))
			Console->Show();
	}

	if (IsActive())
	{
		bool b_is_16_9	= (float)Device.dwWidth / (float)Device.dwHeight > (UI_BASE_WIDTH/UI_BASE_HEIGHT + 0.01f);
		if (b_is_16_9 !=m_activatedScreenRatio)
		{
			ReloadUI();
			m_startDialog->SendMessage(m_startDialog, MAIN_MENU_RELOADED, NULL);
		}
	}
}

void CMainMenu::OnDeviceCreate()
{}

void CMainMenu::Screenshot(IRender_interface::ScreenshotMode mode, LPCSTR name)
{
	if(mode != IRender_interface::SM_FOR_GAMESAVE)
	{
		::Render->Screenshot(mode,name);
	}
	else
	{
		m_Flags.set(flGameSaveScreenshot, TRUE);
		xr_strcpy(m_screenshot_name, name);
		if(g_pGameLevel && m_Flags.test(flActive))
		{
			Device.seqFrame.Add(g_pGameLevel);
			Device.seqRender.Add(g_pGameLevel);
		};
		m_screenshotFrame = Device.dwFrame+1;
		m_Flags.set(flRestoreConsole, Console->bVisible);
		Console->Hide();
	}
}

void CMainMenu::RegisterPPDraw(CUIWindow* w)
{
	UnregisterPPDraw(w);
	m_pp_draw_wnds.push_back(w);
}

void CMainMenu::UnregisterPPDraw(CUIWindow* w)
{
	m_pp_draw_wnds.erase(std::remove(m_pp_draw_wnds.begin(), m_pp_draw_wnds.end(), w), m_pp_draw_wnds.end());
}

void CMainMenu::DestroyInternal(bool bForce)
{
	if (m_startDialog && ((m_deactivated_frame < Device.dwFrame + 4) || bForce))
		xr_delete(m_startDialog);
}

void CMainMenu::OnLoadError(LPCSTR module)
{
	LPCSTR str = CStringTable().translate("ui_st_error_loading").c_str();
	string1024 Text;
	strconcat(sizeof(Text), Text, str, " ");
	xr_strcat(Text,sizeof(Text), module);
}

extern ENGINE_API string512 g_sLaunchOnExit_app;
extern ENGINE_API string512 g_sLaunchOnExit_params;
extern ENGINE_API string_path g_sLaunchWorkingFolder;

void CMainMenu::SetNeedVidRestart()
{
	m_Flags.set(flNeedVidRestart,TRUE);
}

void CMainMenu::OnDeviceReset()
{
	if (IsActive() && g_pGameLevel)
		SetNeedVidRestart();
}

// -------------------------------------------------------------------------------------------------

LPCSTR AddHyphens( LPCSTR c )
{
	static string64 buf;

	u32 sz = xr_strlen(c);
	u32 j = 0; 

	for ( u32 i = 1; i <= 3; ++i )
	{
		buf[i*5 - 1] = '-';
	}

	for ( u32 i = 0; i < sz; ++i )
	{
		j = i + iFloor(i/4.0f);
		buf[j] = c[i];		
	}
	buf[sz + iFloor(sz/4.0f)] = 0;

	return buf;
}

LPCSTR DelHyphens( LPCSTR c )
{
	static string64 buf;

	u32 sz = xr_strlen( c );
	u32 sz1 = _min( iFloor(sz/4.0f), 3 );

	u32 j = 0; 
	for ( u32 i = 0; i < sz - sz1; ++i )
	{
		j = i + iFloor( i/4.0f );
		buf[i] = c[j];		
	}
	buf[sz - sz1] = 0;
	
	return buf;
}

LPCSTR CMainMenu::GetGameVer()
{
	return GAME_VERSION;
}