#include "stdafx.h"
#include "Level.h"
#include "xrServer.h"
#include "actor.h"
#include "game_cl_base.h"
#include "../xrCore/stream_reader.h"
#include "../xrEngine/CameraManager.h"

void CLevel::PrepareToSaveDemo		()
{
	R_ASSERT(!m_DemoPlay);
	string_path demo_name = "";
	string_path demo_path;
	SYSTEMTIME Time;
	GetLocalTime		(&Time);
	xr_sprintf			(demo_name, "xray_%02d-%02d-%02d_%02d-%02d-%02d.demo",
		Time.wMonth,
		Time.wDay,
		Time.wYear,
		Time.wHour,
		Time.wMinute,
		Time.wSecond
	);
	Msg					("Demo would be stored in - %s", demo_name);
	FS.update_path      (demo_path, "$logs$", demo_name);
	m_writer			= FS.w_open(demo_path);
	m_DemoSave			= TRUE;
}

bool CLevel::PrepareToPlayDemo		(shared_str const & file_name)
{
	R_ASSERT(!m_DemoSave);
	m_reader	= FS.rs_open("$logs$", file_name.c_str());
	if (!m_reader)
	{
		Msg("ERROR: failed to open file [%s] to play demo...", file_name.c_str());
		return false;
	}
	if (!LoadDemoHeader())
	{
		Msg("ERROR: bad demo file...");
		return false;
	}
	m_DemoPlay	= TRUE;
	return true;
}

void CLevel::StopSaveDemo()
{
	if (m_writer)
	{
		FS.w_close(m_writer);
	}
}


void CLevel::StartPlayDemo()
{
	m_current_spectator	= NULL;
	m_DemoPlayStarted	= TRUE;
	m_StartGlobalTime	= Device.dwTimeGlobal;
	m_starting_spawns_pos	= 0;
	m_starting_spawns_dtime	= 0;
	Msg("! ------------- Demo Started ------------");
	CatchStartingSpawns	();
}

void CLevel::StopPlayDemo()
{
	if (m_reader)
	{
		//FS.r_close			(m_reader);
		//m_reader			= NULL;
		m_DemoPlayStarted	= FALSE;
		m_DemoPlayStoped	= TRUE;
	}
	Msg("! ------------- Demo Stoped ------------");
}

void CLevel::StartSaveDemo(shared_str const & server_options)
{
	R_ASSERT(IsDemoSave() && !m_DemoSaveStarted);

	SaveDemoHeader		(server_options);
	m_DemoSaveStarted	= TRUE;
}

void CLevel::SaveDemoHeader(shared_str const & server_options)
{
	m_demo_header.m_time_global			= Device.dwTimeGlobal;
	m_demo_header.m_time_server			= timeServer();
	m_demo_header.m_time_delta			= timeServer_Delta();
	m_demo_header.m_time_delta_user		= net_TimeDelta_User;
	m_writer->w(&m_demo_header, sizeof(m_demo_header));
	m_writer->w_stringZ(server_options);
	m_demo_info_file_pos				=	m_writer->tell();
}

void CLevel::SaveDemoInfo()
{
	R_ASSERT(m_writer);
	
	u32 old_pos = m_writer->tell();
	m_writer->seek(m_demo_info_file_pos);
}

void CLevel::SavePacket(NET_Packet& packet)
{
	m_writer->w_u32	(Device.dwTimeGlobal - m_demo_header.m_time_global);
	m_writer->w_u32	(packet.timeReceive);
	m_writer->w_u32	(packet.B.count);
	m_writer->w		(packet.B.data, packet.B.count);
}

bool CLevel::LoadDemoHeader	()
{
	R_ASSERT(m_reader);
	m_reader->r				(&m_demo_header, sizeof(m_demo_header));
	m_reader->r_stringZ		(m_demo_server_options);
	u32 demo_info_start_pos	= m_reader->tell();
	
	return (m_reader->elapsed() >= sizeof(DemoPacket));
}

bool CLevel::LoadPacket		(NET_Packet & dest_packet, u32 global_time_delta)
{
	if (!m_reader || m_reader->eof())
		return false;
	
	m_prev_packet_pos	= m_reader->tell();
	DemoPacket			tmp_hdr;
	
	m_reader->r		(&tmp_hdr, sizeof(DemoPacket));
	m_prev_packet_dtime	= tmp_hdr.m_time_global_delta;
	
	if ( map_data.m_sended_map_name_request ? /// ???
		(tmp_hdr.m_time_global_delta <= global_time_delta) :
		(tmp_hdr.m_time_global_delta < global_time_delta))
	{
		R_ASSERT2	(tmp_hdr.m_packet_size < NET_PacketSizeLimit, "bad demo packet");
		m_reader->r	(dest_packet.B.data, tmp_hdr.m_packet_size);
		dest_packet.B.count		= tmp_hdr.m_packet_size;
		dest_packet.timeReceive = tmp_hdr.m_timeReceive; //not used ..
		dest_packet.r_pos		= 0;
		if (m_reader->elapsed() <= sizeof(DemoPacket))
		{
			StopPlayDemo();
		}
		return true;
	} 
	m_reader->seek(m_prev_packet_pos);
	return false;
}
void CLevel::SimulateServerUpdate()
{
	u32 tdelta = Device.dwTimeGlobal - m_StartGlobalTime;
	NET_Packet tmp_packet;
	while (LoadPacket(tmp_packet, tdelta))
	{
		IPureClient::OnMessage(tmp_packet.B.data, tmp_packet.B.count);
	}
}

void CLevel::SpawnDemoSpectator()
{
	R_ASSERT(Server && Server->game);
	m_current_spectator = NULL;
}

void CLevel::SetDemoSpectator(CObject* spectator)
{
	m_current_spectator = spectator;
}

float CLevel::GetDemoPlayPos() const
{
	if (m_reader->eof())
		return 1.f;
	
	return ( float(m_reader->tell()) / float(m_reader->length()) );
}

void CLevel::CatchStartingSpawns()
{}

void __stdcall CLevel::MSpawnsCatchCallback(u32 message, u32 subtype, NET_Packet & packet)
{}

CObject* CLevel::GetDemoSpectator()	
{ 
	return smart_cast<CGameObject*>(m_current_spectator); 
};
