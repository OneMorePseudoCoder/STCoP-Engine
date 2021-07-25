#include "stdafx.h"
#include "xrMessages.h"
#include "xrGameSpyServer.h"
#include "../xrEngine/igame_persistent.h"

//#define DEMO_BUILD

xrGameSpyServer::xrGameSpyServer()
{
	m_iReportToMasterServer = 0;
	ServerFlags.set( server_flag_all, 0 );
}

xrGameSpyServer::~xrGameSpyServer()
{
}

//----------- xrGameSpyClientData -----------------------
IClient*		xrGameSpyServer::client_Create		()
{
	return xr_new<xrGameSpyClientData> ();
}
xrGameSpyClientData::xrGameSpyClientData	():xrClientData()
{
	m_iCDKeyReauthHint = 0;
}
void	xrGameSpyClientData::Clear()
{
	inherited::Clear();

	m_pChallengeString[0] = 0;
	m_iCDKeyReauthHint = 0;
};

xrGameSpyClientData::~xrGameSpyClientData()
{
	m_pChallengeString[0] = 0;
	m_iCDKeyReauthHint = 0;
}
//-------------------------------------------------------
xrGameSpyServer::EConnect xrGameSpyServer::Connect(shared_str &session_name, GameDescriptionData & game_descr)
{
	EConnect res = inherited::Connect(session_name, game_descr);
	if (res!=ErrNoError) return res;

	if ( 0 == *(game->get_option_s		(*session_name,"hname",NULL)))
	{
		string1024	CompName;
		DWORD		CompNameSize = 1024;
		if (GetComputerName(CompName, &CompNameSize)) 
			HostName	= CompName;
	}
	else
		HostName	= game->get_option_s		(*session_name,"hname",NULL);

	string4096	tMapName = "";
	const char* SName = *session_name;
	strncpy_s(tMapName, *session_name, strchr(SName, '/') - SName);
	MapName		= tMapName;// = (session_name);
	

	m_iReportToMasterServer = game->get_option_i		(*session_name,"public",0);
	m_iMaxPlayers	= game->get_option_i		(*session_name,"maxplayers",32);

	return res;
}

void			xrGameSpyServer::Update				()
{
	inherited::Update();
}

int				xrGameSpyServer::GetPlayersCount()
{
	int NumPlayers = net_players.ClientsCount();
	if (!g_dedicated_server || NumPlayers < 1) return NumPlayers;
	return NumPlayers - 1;
};

void			xrGameSpyServer::OnCL_Disconnected	(IClient* _CL)
{
	inherited::OnCL_Disconnected(_CL);

}

u32				xrGameSpyServer::OnMessage(NET_Packet& P, ClientID sender)			// Non-Zero means broadcasting with "flags" as returned
{
	u16			type;
	P.r_begin	(type);

	xrGameSpyClientData* CL		= (xrGameSpyClientData*)ID_to_client(sender);

	switch (type)
	{
	case M_GAMESPY_CDKEY_VALIDATION_CHALLENGE_RESPOND:
		{
            string128 ResponseStr = "";
            u32 bytesRemain = P.r_elapsed();
            if (bytesRemain == 0 || bytesRemain > sizeof(ResponseStr))
            {
                xr_string clientIp = CL->m_cAddress.to_string();
                Msg("! WARNING: Validation challenge respond from client [%s] is %s. DoS attack?",
                    clientIp.c_str(), bytesRemain == 0 ? "empty" : "too long");
                DisconnectClient(CL, "");
                // XXX nitrocaster: block IP address after X such attempts
                return 0;
            }
            P.r_stringZ(ResponseStr);

			return (0);
		}break;
	}

	return	inherited::OnMessage(P, sender);
};

void xrGameSpyServer::Assign_ServerType( string512& res )
{
	string_path		fn;
	FS.update_path( fn, "$app_data_root$", "server_users.ltx" );
	if( FS.exist(fn) )
	{
		CInifile inif( fn );
		if( inif.section_exist( "users" ) )
		{
			if( inif.line_count( "users" ) != 0 )
			{
				ServerFlags.set( server_flag_protected, 1 );
				xr_strcpy( res, "# Server started as protected, using users list." );
				Msg( res );
				return;
			}else{
				xr_strcpy( res, "Users count in list is null." );
			}
		}else{
			xr_strcpy( res, "Section [users] not found." );
		}
	}else{
		xr_strcpy( res, "File <server_users.ltx> not found in folder <$app_data_root$>." );
	}// if FS.exist(fn)

	Msg( res );
	ServerFlags.set( server_flag_protected, 0 );
	xr_strcpy( res, "# Server started without users list." );
	Msg( res );
}

void xrGameSpyServer::GetServerInfo( CServerInfo* si )
{
	string32 tmp, tmp2;

	si->AddItem( "Server name", HostName.c_str(), RGB(128,128,255) );
	si->AddItem( "Map", MapName.c_str(), RGB(255,0,128) );
	
	xr_strcpy( tmp, itoa( GetPlayersCount(), tmp2, 10 ) );
	xr_strcat( tmp, " / ");
	xr_strcat( tmp, itoa( m_iMaxPlayers, tmp2, 10 ) );
	si->AddItem( "Players", tmp, RGB(255,128,255) );

	string256 res;
	
	xr_strcpy( res, "" );
	if ( xr_strlen( res ) == 0 )
		xr_strcat( res, "free" );
	si->AddItem( "Access to server", res, RGB(200,155,155) );
	inherited::GetServerInfo( si );
}
