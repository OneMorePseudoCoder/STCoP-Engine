// File:		UIMessagesWindow.h
// Description:	Window with MP chat and Game Log ( with PDA messages in single and Kill Messages in MP)
// Created:		22.04.2005
// Author:		Serge Vynnychenko
// Mail:		narrator@gsc-game.kiev.ua
//
// Copyright 2005 GSC Game World

#include "StdAfx.h"
#include "UIMessagesWindow.h"
#include "UIGameLog.h"
#include "xrUIXmlParser.h"
#include "UIXmlInit.h"
#include "UIInventoryUtilities.h"
#include "../game_news.h"
#include "UIPdaMsgListItem.h"

CUIMessagesWindow::CUIMessagesWindow() : m_pGameLog(NULL)
{
	Init(0, 0, UI_BASE_WIDTH, UI_BASE_HEIGHT);
}

CUIMessagesWindow::~CUIMessagesWindow()
{}

void CUIMessagesWindow::AddLogMessage(const shared_str& msg)
{
	m_pGameLog->AddLogMessage(*msg);
}

#define CHAT_LOG_LIST_PENDING "chat_log_list_pending"
void CUIMessagesWindow::Init(float x, float y, float width, float height)
{
	CUIXml xml;
	xml.Load(CONFIG_PATH, UI_PATH, "messages_window.xml");
	m_pGameLog = xr_new<CUIGameLog>();
	m_pGameLog->SetAutoDelete(true);
	m_pGameLog->Show(true);
	AttachChild(m_pGameLog);
	CUIXmlInit::InitScrollView(xml, "sp_log_list", 0, m_pGameLog);
}

void CUIMessagesWindow::AddIconedPdaMessage(GAME_NEWS_DATA* news)
{
	CUIPdaMsgListItem *pItem = m_pGameLog->AddPdaMessage();
	
	LPCSTR time_str = InventoryUtilities::GetTimeAsString( news->receive_time, InventoryUtilities::etpTimeToMinutes ).c_str();
	pItem->UITimeText.SetText(time_str);
	pItem->UITimeText.AdjustWidthToText();
	Fvector2 p = pItem->UICaptionText.GetWndPos();
	p.x	= pItem->UITimeText.GetWndPos().x + pItem->UITimeText.GetWidth() + 3.0f;
	pItem->UICaptionText.SetWndPos(p);
	pItem->UICaptionText.SetTextST(news->news_caption.c_str());
	pItem->UIMsgText.SetTextST(news->news_text.c_str());
	pItem->UIMsgText.AdjustHeightToText	();

    pItem->SetColorAnimation("ui_main_msgs_short", LA_ONLYALPHA|LA_TEXTCOLOR|LA_TEXTURECOLOR, float(news->show_time));
	pItem->UIIcon.InitTexture(news->texture_name.c_str());
	
	float h1 = _max(pItem->UIIcon.GetHeight(), pItem->UIMsgText.GetWndPos().y + pItem->UIMsgText.GetHeight());
	pItem->SetHeight(h1 + 3.0f);

	m_pGameLog->SendMessage(pItem,CHILD_CHANGED_SIZE);
}

void CUIMessagesWindow::Show(bool show)
{
	if (m_pGameLog)
		m_pGameLog->Show(show);
}
