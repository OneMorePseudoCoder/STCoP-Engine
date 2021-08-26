#include "stdafx.h"
#include "UIGameMP.h"
#include "UIAchivementsIndicator.h"
#include "ui/UIServerInfo.h"
#include "UICursor.h"
#include "Level.h"

UIGameMP::UIGameMP() :
	m_pServerInfo(NULL),
	m_pAchivementIdicator(NULL)
{
}

UIGameMP::~UIGameMP()
{
	xr_delete	(m_pServerInfo);
}
#include <dinput.h>

bool UIGameMP::IR_UIOnKeyboardPress(int dik)
{
	return inherited::IR_UIOnKeyboardPress(dik);
}

bool UIGameMP::IR_UIOnKeyboardRelease(int dik)
{
	return inherited::IR_UIOnKeyboardRelease(dik);
}

bool UIGameMP::IsServerInfoShown	()
{
	VERIFY(m_pServerInfo);
	return m_pServerInfo->IsShown();
}

//shows only if it has some info ...
bool UIGameMP::ShowServerInfo()
{
	VERIFY2(m_pServerInfo, "game client UI not created");
	if (!m_pServerInfo)
	{
		return false;
	}
	
	if (!m_pServerInfo->HasInfo())
	{
		return true;
	}

	if (!m_pServerInfo->IsShown())
	{
		m_pServerInfo->ShowDialog(true);
	}
	return true;
}

void UIGameMP::SetClGame(game_cl_GameState* g)
{
	inherited::SetClGame(g);

	if (m_pServerInfo)
	{
		if (m_pServerInfo->IsShown())
			m_pServerInfo->HideDialog();

		xr_delete(m_pServerInfo);
	}
	m_pServerInfo			= xr_new<CUIServerInfo>();
	m_pAchivementIdicator	= xr_new<CUIAchivementIndicator>();
	m_pAchivementIdicator->SetAutoDelete(true);
	Window->AttachChild	(m_pAchivementIdicator);
	
}

void UIGameMP::SetServerLogo(u8 const * data_ptr, u32 data_size)
{
	VERIFY(m_pServerInfo);
	m_pServerInfo->SetServerLogo(data_ptr, data_size);
}
void UIGameMP::SetServerRules(u8 const * data_ptr, u32 data_size)
{
	VERIFY(m_pServerInfo);
	m_pServerInfo->SetServerRules(data_ptr, data_size);
}

void UIGameMP::AddAchivment(shared_str const & achivement_name,
							shared_str const & color_animation,
							u32 const width,
							u32 const height)
{
	VERIFY(m_pAchivementIdicator);
	m_pAchivementIdicator->AddAchivement(achivement_name, color_animation, width, height);
}



