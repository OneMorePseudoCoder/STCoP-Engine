#include "stdafx.h"
#include "player_team_win_score.h"
#include "game_state_accumulator.h"

namespace award_system
{

player_team_win_score::player_team_win_score(game_state_accumulator* owner) :
	inherited(owner)
{
	m_win_score		= 0;
};

void player_team_win_score::reset_game()
{
	m_win_score		= 0;
}

void player_team_win_score::OnRoundStart()
{
	reset_game();
}

void player_team_win_score::OnRoundEnd()
{
	save_round_scores();
}

void player_team_win_score::save_round_scores()
{
	m_green_team_score	= 0;
	m_blue_team_score	= 0;

	game_PlayerState* tmp_local_player = m_owner->get_local_player();
	if (!tmp_local_player)
		return;
}

player_enemy_team_score::player_enemy_team_score(game_state_accumulator* owner) :
	inherited(owner)
{
	m_enemy_team_score = 0;
}

void player_enemy_team_score::reset_game()
{
	inherited::reset_game	();
	m_enemy_team_score		= 0;
}

void player_enemy_team_score::OnRoundEnd()
{
	save_round_scores		();
}

void player_enemy_team_score::save_round_scores()
{
	inherited::save_round_scores();
}

player_runtime_win_score::player_runtime_win_score(game_state_accumulator* owner) :
	inherited(owner)
{
}

u32 const player_runtime_win_score::get_u32_param()
{
	u32 ret_score = 0;

	return ret_score;
}

void player_runtime_win_score::OnPlayerBringArtefact(game_PlayerState const * ps)
{
	save_round_scores();
}

player_runtime_enemy_team_score::player_runtime_enemy_team_score(game_state_accumulator* owner) :
	inherited(owner)
{
}

void player_runtime_enemy_team_score::OnPlayerBringArtefact(game_PlayerState const * ps)
{
	save_round_scores();
}

} //namespace award_system