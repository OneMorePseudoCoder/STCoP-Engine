#include "stdafx.h"
#include "actor_mp_server.h"

CSE_ActorMP::CSE_ActorMP		(LPCSTR section) : 
	inherited				(section)
{
	m_ready_to_update		= false;
}

void CSE_ActorMP::STATE_Read	(NET_Packet &packet, u16 size)
{
	inherited::STATE_Read	(packet,size);
}

void CSE_ActorMP::STATE_Write	(NET_Packet &packet)
{
	inherited::STATE_Write	(packet);
}

BOOL CSE_ActorMP::Net_Relevant	()
{
	if (get_health() <= 0) return (false);
	return (inherited::Net_Relevant());
}

#ifdef XRGAME_EXPORTS
void	CSE_ActorMP::on_death				(CSE_Abstract *killer)
{
	inherited::on_death(killer);
}
#endif