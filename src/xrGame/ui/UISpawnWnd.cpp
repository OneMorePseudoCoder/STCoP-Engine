#include "stdafx.h"
#include <dinput.h>
#include "UISpawnWnd.h"
#include "UIXmlInit.h"
#include "../level.h"
#include "UIStatix.h"
#include "UIScrollView.h"
#include "UI3tButton.h"
#include "../xr_level_controller.h"
#include "uicursor.h"
#include "uigamecustom.h"

CUISpawnWnd::CUISpawnWnd()
	:  m_iCurTeam(0)
{	
	m_pBackground	= xr_new<CUIStatic>();	AttachChild(m_pBackground);	
	m_pCaption		= xr_new<CUIStatic>();	AttachChild(m_pCaption);	
	m_pImage1		= xr_new<CUIStatix>();	AttachChild(m_pImage1);
	m_pImage2		= xr_new<CUIStatix>();	AttachChild(m_pImage2);

	m_pFrames[0]	= xr_new<CUIStatic>();	AttachChild(m_pFrames[0]);
	m_pFrames[1]	= xr_new<CUIStatic>();	AttachChild(m_pFrames[1]);
//	m_pFrames[2]	= xr_new<CUIStatic>();	AttachChild(m_pFrames[2]);

	m_pTextDesc		= xr_new<CUIScrollView>();	AttachChild(m_pTextDesc);

	m_pBtnAutoSelect= xr_new<CUI3tButton>();	AttachChild(m_pBtnAutoSelect);
	m_pBtnSpectator	= xr_new<CUI3tButton>();	AttachChild(m_pBtnSpectator);
	m_pBtnBack		= xr_new<CUI3tButton>();	AttachChild(m_pBtnBack);
	
	Init();	
}

CUISpawnWnd::~CUISpawnWnd()
{
	xr_delete(m_pCaption);
	xr_delete(m_pBackground);
	xr_delete(m_pFrames[0]);
	xr_delete(m_pFrames[1]);
//	xr_delete(m_pFrames[2]);
	xr_delete(m_pImage1);
	xr_delete(m_pImage2);
	xr_delete(m_pTextDesc);
	xr_delete(m_pBtnAutoSelect);
	xr_delete(m_pBtnSpectator);
	xr_delete(m_pBtnBack);	
}



void CUISpawnWnd::Init()
{
	CUIXml xml_doc;
	xml_doc.Load(CONFIG_PATH, UI_PATH, "spawn.xml");

	CUIXmlInit::InitWindow(xml_doc,"team_selector",						0,	this);
	CUIXmlInit::InitStatic(xml_doc,"team_selector:caption",				0,	m_pCaption);
	CUIXmlInit::InitStatic(xml_doc,"team_selector:background",			0,	m_pBackground);
	CUIXmlInit::InitStatic(xml_doc,"team_selector:image_frames_tl",		0,	m_pFrames[0]);
	CUIXmlInit::InitStatic(xml_doc,"team_selector:image_frames_tr",		0,	m_pFrames[1]);
//	CUIXmlInit::InitStatic(xml_doc,"team_selector:image_frames_bottom",	0,	m_pFrames[2]);
	CUIXmlInit::InitScrollView(xml_doc,"team_selector:text_desc",			0,	m_pTextDesc);

	CUIXmlInit::InitStatic(xml_doc,"team_selector:image_0",0,m_pImage1);
	//m_pImage1->SetStretchTexture(true);	
	CUIXmlInit::InitStatic(xml_doc,"team_selector:image_1",0,m_pImage2);
	//m_pImage2->SetStretchTexture(true);
	//InitTeamLogo();

	CUIXmlInit::Init3tButton(xml_doc,"team_selector:btn_spectator",	0,m_pBtnSpectator);
	CUIXmlInit::Init3tButton(xml_doc,"team_selector:btn_autoselect",0,m_pBtnAutoSelect);
	CUIXmlInit::Init3tButton(xml_doc,"team_selector:btn_back",		0,m_pBtnBack);
}

void CUISpawnWnd::InitTeamLogo(){
	R_ASSERT(pSettings->section_exist("team_logo"));
	R_ASSERT(pSettings->line_exist("team_logo", "team1"));
	R_ASSERT(pSettings->line_exist("team_logo", "team2"));

	m_pImage1->InitTexture(pSettings->r_string("team_logo", "team1"));
	m_pImage2->InitTexture(pSettings->r_string("team_logo", "team2"));
}

void CUISpawnWnd::SendMessage(CUIWindow *pWnd, s16 msg, void *pData)
{
	if (BUTTON_CLICKED == msg)
	{
		HideDialog							();
	}

	inherited::SendMessage(pWnd, msg, pData);
}

////////////////////////////////////////////////////////////////////////////////

bool CUISpawnWnd::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
	if (WINDOW_KEY_PRESSED != keyboard_action)
	{
		if (dik == DIK_TAB)
		{
			ShowChildren(true);
			UI().GetUICursor().Show();
		}		
		return false;
	}

	if (dik == DIK_TAB)
	{
        ShowChildren(false);
		UI().GetUICursor().Hide();
		return false;
	}

	switch (dik)
	{
	case DIK_ESCAPE:
		HideDialog							();
		return true;
	}

	return inherited::OnKeyboardAction(dik, keyboard_action);
}

void CUISpawnWnd::SetVisibleForBtn(ETEAMMENU_BTN btn, bool state){
	switch (btn)
	{
	case 	TEAM_MENU_BACK:			this->m_pBtnBack->SetVisible(state);		break;
	case	TEAM_MENU_SPECTATOR:	this->m_pBtnSpectator->SetVisible(state);	break;		
	case	TEAM_MENU_AUTOSELECT:	this->m_pBtnAutoSelect->SetVisible(state);	break;
	default:
		R_ASSERT2(false,"invalid btn ID");	
	}
}

void CUISpawnWnd::SetCurTeam(int team){
	R_ASSERT2(team >= -1 && team <= 1, "Invalid team number");

    m_iCurTeam = team;
	m_pImage1->SetSelectedState(0 == team ? true : false);
	m_pImage2->SetSelectedState(1 == team ? true : false);
}

