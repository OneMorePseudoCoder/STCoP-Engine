#include "stdafx.h"
#include "xrserver.h"
#include "game_sv_single.h"
#include "xrMessages.h"
#include "game_cl_single.h"
#include "MainMenu.h"
#include "../xrEngine/x_ray.h"
#include "../xrNetServer/NET_AuthCheck.h"
#pragma warning(push)
#pragma warning(disable:4995)
#include <malloc.h>
#pragma warning(pop)

LPCSTR xrServer::get_map_download_url(LPCSTR level_name, LPCSTR level_version)
{
	R_ASSERT(level_name && level_version);
	LPCSTR ret_url = "";
	CInifile* level_ini = pApp->GetArchiveHeader(level_name, level_version);
	if (!level_ini)
	{
		return ret_url;
	}

	ret_url = level_ini->r_string_wb("header", "link").c_str();
	if (!ret_url)
		ret_url = "";
	
	return ret_url;
}

xrServer::EConnect xrServer::Connect(shared_str &session_name, GameDescriptionData & game_descr)
{
#ifdef DEBUG
	Msg						("* sv_Connect: %s",	*session_name);
#endif

	// Parse options and create game
	if (0==strchr(*session_name,'/'))
		return				ErrConnect;

	string1024				options;
	R_ASSERT2(xr_strlen(session_name) <= sizeof(options), "session_name too BIIIGGG!!!");
	xr_strcpy					(options,strchr(*session_name,'/')+1);
	
	// Parse game type
	string1024				type;
	R_ASSERT2(xr_strlen(options) <= sizeof(type), "session_name too BIIIGGG!!!");
	xr_strcpy					(type,options);
	if (strchr(type,'/'))	*strchr(type,'/') = 0;
	game					= NULL;

	CLASS_ID clsid			= game_GameState::getCLASS_ID(type,true);
	game					= smart_cast<game_sv_GameState*> (NEW_INSTANCE(clsid));
	
	// Options
	if (0 == game)			
		return ErrConnect;

#ifdef DEBUG
	Msg("* Created server_game %s",game->type_name());
#endif
	
	ZeroMemory(&game_descr, sizeof(game_descr));
	xr_strcpy(game_descr.map_name, game->level_name(session_name.c_str()).c_str());
	xr_strcpy(game_descr.map_version, game_sv_GameState::parse_level_version(session_name.c_str()).c_str());
	xr_strcpy(game_descr.download_url, get_map_download_url(game_descr.map_name, game_descr.map_version));

	game->Create			(session_name);

	return IPureServer::Connect(*session_name, game_descr);
}

IClient* xrServer::new_client( SClientConnectData* cl_data )
{
	IClient* CL		= client_Find_Get( cl_data->clientID );
	VERIFY( CL );
	
	// copy entity
	CL->ID			= cl_data->clientID;
	CL->process_id	= cl_data->process_id;
	CL->name		= cl_data->name;	//only for offline mode
	CL->pass._set	( cl_data->pass );

	NET_Packet		P;
	P.B.count		= 0;
	P.r_pos			= 0;

	game->AddDelayedEvent( P, GAME_EVENT_CREATE_CLIENT, 0, CL->ID );
	
	return CL;
}

void xrServer::AttachNewClient			(IClient* CL)
{
	MSYS_CONFIG	msgConfig;
	msgConfig.sign1 = 0x12071980;
	msgConfig.sign2 = 0x26111975;

	if(psNET_direct_connect) //single_game
	{
        SV_Client			= CL;
		CL->flags.bLocal	= 1;
		SendTo_LL( SV_Client->ID, &msgConfig, sizeof(msgConfig), net_flags(TRUE,TRUE,TRUE,TRUE) );
	} 
	else
	{
		SendTo_LL				(CL->ID,&msgConfig,sizeof(msgConfig), net_flags(TRUE, TRUE, TRUE, TRUE));
		Server_Client_Check		(CL); 
	}

	RequestClientDigest(CL);

	CL->m_guid[0]=0;
}

void xrServer::RequestClientDigest(IClient* CL)
{
	if (CL == GetServerClient())
	{
		Check_BuildVersion_Success(CL);	
		return;
	}
	xrClientData* tmp_client	= smart_cast<xrClientData*>(CL);
	VERIFY						(tmp_client);
	PerformSecretKeysSync		(tmp_client);

	NET_Packet P;
	P.w_begin					(M_SV_DIGEST);
	SendTo						(CL->ID, P);
}

void xrServer::ProcessClientDigest(xrClientData* xrCL, NET_Packet* P)
{
	R_ASSERT(xrCL);
	IClient* tmp_client = static_cast<IClient*>(xrCL);
	P->r_stringZ(xrCL->m_cdkey_digest);

	GetPooledState				(xrCL);
	PerformSecretKeysSync		(xrCL);
	Check_BuildVersion_Success	(tmp_client);	
}