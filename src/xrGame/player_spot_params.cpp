#include "stdafx.h"
#include "player_spot_params.h"
#include "game_base.h"
#include "game_state_accumulator.h"

namespace award_system
{

u32 const calculate_spots(game_PlayerState* ps)
{
	return ps->m_iRivalKills - (ps->m_iTeamKills * 2) - ps->m_iSelfKills + (ps->af_count * 3);
};

player_spots_counter::player_spots_counter(game_state_accumulator* owner) :
	inherited(owner)
{
}

u32 const player_spots_counter::get_u32_param()
{
	game_PlayerState* tmp_local_player = m_owner->get_local_player();
	if (tmp_local_player)
		return calculate_spots(tmp_local_player);
	
	return 0;
}

u32 const player_spots_with_top_enemy_divider::get_top_enemy_player_score()
{
	game_PlayerState* tmp_local_player = m_owner->get_local_player();
	if (!tmp_local_player)
		return 0;

	s32	max_score = 0;
	
	return max_score;
}

float const player_spots_with_top_enemy_divider::get_float_param()
{
	float my_spots_count = static_cast<float>(player_spots_counter::get_u32_param());
	u32 top_enemy = get_top_enemy_player_score();
	if (top_enemy == 0)
	{
		return my_spots_count;
	}
	return my_spots_count / top_enemy;
}

} //namespace award_system