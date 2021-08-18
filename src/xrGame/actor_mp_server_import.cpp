#include "stdafx.h"
#include "actor_mp_server.h"
//#include "Physics.h"
//#include "mathutils.h"
#include "../xrphysics/phvalide.h"

void CSE_ActorMP::UPDATE_Read	(NET_Packet &packet)
{
	flags						= 0;
	m_u16NumItems				= 1;
	velocity.set				(0.f,0.f,0.f);

	if (get_health() <= 0)
	{
		return;
	}

	m_ready_to_update			= true;
}
