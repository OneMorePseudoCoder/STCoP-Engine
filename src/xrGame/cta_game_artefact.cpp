////////////////////////////////////////////////////////////////////////////////
//	Module		:	cta_game_artefact.cpp
//	Created		:	19.12.2007
//	Modified	:	19.12.2007
//	Autor		:	Alexander Maniluk
//	Description	:	Artefact object for Capture The Artefact game mode
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "cta_game_artefact.h"
#include "cta_game_artefact_activation.h"
#include "xrServer_Objects_Alife_Items.h"
#include "xr_level_controller.h"

CtaGameArtefact::CtaGameArtefact()
{
	// game object must present...
	m_artefact_rpoint	= NULL;
	m_my_team			= etSpectatorsTeam;
}

CtaGameArtefact::~CtaGameArtefact()
{
}
bool CtaGameArtefact::IsMyTeamArtefact()
{
	R_ASSERT			(H_Parent());

	return false;
}
bool CtaGameArtefact::Action(s32 cmd, u32 flags)
{
	return inherited::Action((u16)cmd, flags);
}

void CtaGameArtefact::OnStateSwitch(u32 S)
{
	inherited::OnStateSwitch(S);
}

void CtaGameArtefact::OnAnimationEnd(u32 state)
{
	if (!H_Parent())
	{
#ifndef MASTER_GOLD
		Msg("! ERROR: enemy artefact activation, H_Parent is NULL.");
#endif // #ifndef MASTER_GOLD
		return;
	}
	inherited::OnAnimationEnd(state);
}

void CtaGameArtefact::UpdateCLChild()
{
	inherited::UpdateCLChild();
	if(H_Parent())
		XFORM().set(H_Parent()->XFORM());

	if (!m_artefact_rpoint)
		InitializeArtefactRPoint();
	
	if (!m_artefact_rpoint)
	{
#ifdef DEBUG
		Msg("--- Waiting for sync packet, for artefact rpoint.");
#endif // #ifdef DEBUG
		return;
	}

	VERIFY(m_artefact_rpoint);
}

void CtaGameArtefact::InitializeArtefactRPoint()
{

}

void CtaGameArtefact::CreateArtefactActivation()
{
}

bool CtaGameArtefact::CanTake() const
{
	if (!inherited::CanTake())
		return false;
	
	return true;
};

void CtaGameArtefact::PH_A_CrPr()
{

}

