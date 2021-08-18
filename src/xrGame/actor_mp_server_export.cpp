#include "stdafx.h"
#include "actor_mp_server.h"
//#include "Physics.h"
//#include "mathutils.h"
#include "../xrphysics/phvalide.h"


void CSE_ActorMP::fill_state	()
{
	m_ready_to_update				= true;
}

void CSE_ActorMP::UPDATE_Write	(NET_Packet &packet)
{
}
