#include "stdafx.h"
#include "rewarding_state_events.h"
#include "game_state_accumulator.h"
#include "ammunition_groups.h"
#include "bone_groups.h"

namespace award_system
{

rewarding_state_events::rewarding_state_events(game_state_accumulator* pstate_accum,
								   event_action_delegate_t ea_delegate) :
	inherited(pstate_accum, ea_delegate)
{
}

rewarding_state_events::~rewarding_state_events()
{
}

void rewarding_state_events::init()
{
	clear_events();
}

} //namespace award_system
