#include "stdafx.h"
#include "../xrEngine/xr_ioconsole.h"
#include "../xrEngine/xr_ioc_cmd.h"
#include "level.h"
#include "xrServer.h"
#include "game_cl_base.h"
#include "actor.h"
#include "xrServer_Object_base.h"
#include "gamepersistent.h"
#include "MainMenu.h"
#include "UIGameCustom.h"
#include "date_time.h"
#include "game_cl_base_weapon_usage_statistic.h"
#include "string_table.h"

EGameIDs	ParseStringToGameType	(LPCSTR str);
LPCSTR		GameTypeToString		(EGameIDs gt, bool bShort);
LPCSTR		AddHyphens				(LPCSTR c);
LPCSTR		DelHyphens				(LPCSTR c);

extern	float	g_cl_lvInterp;
extern	int		g_cl_InterpolationType; //0 - Linear, 1 - BSpline, 2 - HSpline
extern	u32		g_cl_InterpolationMaxPoints;
extern	int		g_cl_save_demo;
extern	float	g_fTimeFactor;
extern	BOOL	g_b_COD_PickUpMode		;
extern	int		g_iWeaponRemove			;
extern	int		g_iCorpseRemove			;
extern	BOOL	g_bCollectStatisticData ;

extern	int		g_Dump_Update_Write;
extern	int		g_Dump_Update_Read;
extern	int		g_be_message_out;

extern	u32		g_sv_Client_Reconnect_Time;
		int		g_dwEventDelay			= 0	;

void XRNETSERVER_API DumpNetCompressorStats	(bool brief);

class CCC_Restart : public IConsole_Command {
public:
					CCC_Restart		(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (!OnServer())			return;
		if(Level().Server)
		{
			Level().Server->game->round_end_reason = eRoundEnd_GameRestarted;
			Level().Server->game->OnRoundEnd();
		}
	}
	virtual void	Info	(TInfo& I){xr_strcpy(I,"restart game");}
};

class CCC_RestartFast : public IConsole_Command {
public:
					CCC_RestartFast	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (!OnServer())		
									return;
		if(Level().Server)
		{
			Level().Server->game->round_end_reason = eRoundEnd_GameRestartedFast;
			Level().Server->game->OnRoundEnd();
		}
	}
	virtual void	Info			(TInfo& I) {xr_strcpy(I,"restart game fast");}
};

class CCC_Kill : public IConsole_Command {
public:
					CCC_Kill		(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (IsGameTypeSingle())		
										return;
		
		if (Game().local_player && 
			Game().local_player->testFlag(GAME_PLAYER_FLAG_VERY_VERY_DEAD)) 
										return;
		
		CObject *l_pObj					= Level().CurrentControlEntity();
		CActor *l_pPlayer				= smart_cast<CActor*>(l_pObj);
		if(l_pPlayer) 
		{
			NET_Packet					P;
			l_pPlayer->u_EventGen		(P,GE_GAME_EVENT,l_pPlayer->ID()	);
			P.w_u16						(GAME_EVENT_PLAYER_KILL);
			P.w_u16						(u16(l_pPlayer->ID())	);
			l_pPlayer->u_EventSend		(P);
		}
	}
	virtual void	Info	(TInfo& I)	{ xr_strcpy(I,"player kill"); }
};

class CCC_Net_CL_Resync : public IConsole_Command {
public:
						CCC_Net_CL_Resync	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void		Execute				(LPCSTR args) 
	{
		Level().net_Syncronize();
	}
	virtual void	Info	(TInfo& I)		{xr_strcpy(I,"resyncronize client");}
};

class CCC_Net_CL_ClearStats : public IConsole_Command {
public:
						CCC_Net_CL_ClearStats	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void		Execute					(LPCSTR args)
	{
		Level().ClearStatistic();
	}
	virtual void		Info	(TInfo& I){xr_strcpy(I,"clear client net statistic");}
};

class CCC_Net_SV_ClearStats : public IConsole_Command {
public:
						CCC_Net_SV_ClearStats	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void		Execute					(LPCSTR args) 
	{
		Level().Server->ClearStatistic();
	}
	virtual void	Info	(TInfo& I){xr_strcpy(I,"clear server net statistic"); }
};

#ifdef DEBUG
class CCC_Dbg_NumObjects : public IConsole_Command {
public:
						CCC_Dbg_NumObjects	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void		Execute				(LPCSTR args) 
	{
		
		u32 SVObjNum	= (OnServer()) ? Level().Server->GetEntitiesNum() : 0;
		xr_vector<u16>	SObjID;
		for (u32 i=0; i<SVObjNum; i++)
		{
			CSE_Abstract* pEntity = Level().Server->GetEntity(i);
			SObjID.push_back(pEntity->ID);
		};
		std::sort(SObjID.begin(), SObjID.end());

		u32 CLObjNum	= Level().Objects.o_count();
		xr_vector<u16>	CObjID;
		for (i=0; i<CLObjNum; i++)
		{
			CObjID.push_back(Level().Objects.o_get_by_iterator(i)->ID());
		};
		std::sort(CObjID.begin(), CObjID.end());

		Msg("Client Objects : %d", CLObjNum);
		Msg("Server Objects : %d", SVObjNum);

		for (u32 CO= 0; CO<_max(CLObjNum, SVObjNum); CO++)
		{
			if (CO < CLObjNum && CO < SVObjNum)
			{
				CSE_Abstract* pEntity = Level().Server->ID_to_entity(SObjID[CO]);
				CObject* pObj = Level().Objects.net_Find(CObjID[CO]);
				char color = (pObj->ID() == pEntity->ID) ? '-' : '!';

				Msg("%c%4d: Client - %20s[%5d] <===> Server - %s [%d]", color, CO+1, 
					*(pObj->cNameSect()), pObj->ID(),
					pEntity->s_name.c_str(), pEntity->ID);
			}
			else
			{
				if (CO<CLObjNum)
				{
					CObject* pObj = Level().Objects.net_Find(CObjID[CO]);
					Msg("! %2d: Client - %s [%d] <===> Server - -----------------", CO+1, 
						*(pObj->cNameSect()), pObj->ID());
				}
				else
				{
					CSE_Abstract* pEntity = Level().Server->ID_to_entity(SObjID[CO]);
					Msg("! %2d: Client - ----- <===> Server - %s [%d]", CO+1, 
						pEntity->s_name.c_str(), pEntity->ID);
				}
			}
		};

		Msg("Client Objects : %d", CLObjNum);
		Msg("Server Objects : %d", SVObjNum);
	}
	virtual void	Info	(TInfo& I){xr_strcpy(I,"dbg Num Objects"); }
};
#endif // DEBUG

//most useful predicates 
struct SearcherClientByName
{
	string512 player_name;
	SearcherClientByName(LPCSTR name)
	{
		strncpy_s(player_name, sizeof(player_name), name, sizeof(player_name) - 1);
		xr_strlwr(player_name);
	}
	bool operator()(IClient* client)
	{
		xrClientData*	temp_client = smart_cast<xrClientData*>(client);
		LPSTR tmp_player = NULL;
		if (!temp_client->ps)
			return false;

		return false;
	}
};

#define RAPREFIX "raid:"
static xrClientData* exclude_command_initiator(LPCSTR args)
{
	LPCSTR tmp_str = strrchr(args, ' ');
	if (!tmp_str)
		tmp_str = args;
	LPCSTR clientidstr = strstr(tmp_str, RAPREFIX);
	if (clientidstr)
	{
		clientidstr += sizeof(RAPREFIX) - 1;
		u32 client_id = static_cast<u32>(strtoul(clientidstr, NULL, 10));
		ClientID tmp_id;
		tmp_id.set(client_id);
		if (g_pGameLevel && Level().Server)
			return Level().Server->ID_to_client(tmp_id);
	}
	return NULL;
};
static char const * exclude_raid_from_args(LPCSTR args, LPSTR dest, size_t dest_size)
{
	strncpy_s(dest, dest_size, args, dest_size - 1);
	char* tmp_str = strrchr(dest, ' ');
	if (!tmp_str)
		tmp_str = dest;
	char* raidstr = strstr(tmp_str, RAPREFIX);
	if (raidstr)
	{
		if (raidstr <= tmp_str)
			*raidstr		= 0;
		else
			*(raidstr - 1)	= 0;	//with ' '
	}
	dest[dest_size - 1] = 0;
	return dest;
}

class CCC_MakeScreenshot : public IConsole_Command {
public:
	CCC_MakeScreenshot (LPCSTR N) : IConsole_Command(N) { bEmptyArgsHandled = false; };
	virtual void	Execute		(LPCSTR args_) 
	{
		if (!g_pGameLevel || !Level().Server || !Level().Server->game) return;
		u32 len	= xr_strlen(args_);
		if ((len == 0) || (len >= 256))		//two digits and raid:%u
			return;

		ClientID client_id = 0;
		{
			u32 tmp_client_id;
			if (sscanf_s(args_, "%u", &tmp_client_id) != 1)
			{
				Msg("! ERROR: bad command parameters.");
				return;
			}
			client_id.set(tmp_client_id);
		}
		xrClientData* admin_client = exclude_command_initiator(args_);
		if (!admin_client)
		{
			Msg("! ERROR: only radmin can make screenshots ...");
			return;
		}
		Level().Server->MakeScreenshot(admin_client->ID, client_id);
	}
	virtual void	Info		(TInfo& I)
	{
		xr_strcpy(I, 
			make_string(
				"Make screenshot. Format: \"make_screenshot <player session id | \'%s\'> <ban_time_in_sec>\". To receive list of players ids see sv_listplayers"
			).c_str()
		);
	}

}; //class CCC_MakeScreenshot

class CCC_MakeConfigDump : public IConsole_Command {
public:
	CCC_MakeConfigDump(LPCSTR N) : IConsole_Command(N) { bEmptyArgsHandled = false; };
	virtual void	Execute		(LPCSTR args_) 
	{
		if (!g_pGameLevel || !Level().Server || !Level().Server->game) return;
		u32 len	= xr_strlen(args_);
		if ((len == 0) || (len >= 256))		//two digits and raid:%u
			return;

		ClientID client_id = 0;
		{
			u32 tmp_client_id;
			if (sscanf_s(args_, "%u", &tmp_client_id) != 1)
			{
				Msg("! ERROR: bad command parameters.");
				Msg("Make screenshot. Format: \"make_config_dump <player session id | \'%s\'> <ban_time_in_sec>\". To receive list of players ids see sv_listplayers");
				return;
			}
			client_id.set(tmp_client_id);
		}
		xrClientData* admin_client = exclude_command_initiator(args_);
		if (!admin_client)
		{
			Msg("! ERROR: only radmin can make config dumps ...");
			return;
		}
		Level().Server->MakeConfigDump(admin_client->ID, client_id);
	}
	virtual void	Info		(TInfo& I)
	{
		xr_strcpy(I, 
			make_string(
				"Make config dump. Format: \"make_config_dump <player session id | \'%s\'> <ban_time_in_sec>\". To receive list of players ids see sv_listplayers"
			).c_str()
		);
	}

}; //class CCC_MakeConfigDump



class CCC_SetDemoPlaySpeed : public IConsole_Command {
public:
					CCC_SetDemoPlaySpeed	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
		float new_speed;
		sscanf(args, "%f", &new_speed);
		Level().SetDemoPlaySpeed(new_speed);
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Set demo play speed (0.0, 8.0]"); }
}; //class CCC_SetDemoPlaySpeed

class DemoPlayControlArgParser
{
protected:
	shared_str					m_action_param;
	bool	ParseControlString		(LPCSTR args_string)
	{
		string16		action_name;
		action_name[0]	= 0;
		string32		param_name;
		param_name[0]	= 0;

		sscanf_s(args_string, "%16s %32s",
			action_name, sizeof(action_name),
			param_name, sizeof(param_name));
		m_action_param = param_name;

		return true;
	};
	inline LPCSTR GetInfoString()
	{
		return "<roundstart,kill,die,artefacttake,artefactdrop,artefactdeliver> [player name]";
	}
}; //class DemoPlayControlArgParser

class CCC_DemoPlayPauseOn :
	public IConsole_Command,
	public DemoPlayControlArgParser
{
public:
					CCC_DemoPlayPauseOn		(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
		if (!ParseControlString(args))
		{
			TInfo tmp_info;
			Info(tmp_info);
			Msg(tmp_info);
			return;
		}
	};

	virtual void	Info	(TInfo& I)
	{
		LPCSTR info_str = NULL;
		STRCONCAT(info_str,
			"Play demo until specified event (then pause playing). Format: mpdemoplay_pause_on ",
			DemoPlayControlArgParser::GetInfoString());
		xr_strcpy(I, info_str);
	}
}; //class CCC_DemoPlayPauseOn

class CCC_DemoPlayCancelPauseOn : public IConsole_Command {
public:
					CCC_DemoPlayCancelPauseOn	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Cancels mpdemoplay_pause_on."); }
}; //class CCC_DemoPlayCancelPauseOn

class CCC_DemoPlayRewindUntil :
	public IConsole_Command,
	public DemoPlayControlArgParser
{
public:
					CCC_DemoPlayRewindUntil	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
		if (!ParseControlString(args))
		{
			TInfo tmp_info;
			Info(tmp_info);
			Msg(tmp_info);
			return;
		}
	};

	virtual void	Info	(TInfo& I)
	{
		LPCSTR info_str = NULL;
		STRCONCAT(info_str,
			"Rewind demo until specified event (then pause playing). Format: mpdemoplay_rewind_until ",
			DemoPlayControlArgParser::GetInfoString());
		xr_strcpy(I, info_str);
	}
}; //class CCC_DemoPlayRewindUntil

class CCC_DemoPlayStopRewind : public IConsole_Command {
public:
					CCC_DemoPlayStopRewind	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Stops rewinding (mpdemoplay_rewind_until)."); }
}; //class CCC_DemoPlayStopRewind

class CCC_DemoPlayRestart : public IConsole_Command {
public:
					CCC_DemoPlayRestart	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!Level().IsDemoPlay())
		{
			Msg("! No demo play started.");
			return;
		}
		Level().RestartPlayDemo();
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Restarts playing demo."); }
}; //class CCC_DemoPlayRestart



class CCC_MulDemoPlaySpeed : public IConsole_Command {
public:
					CCC_MulDemoPlaySpeed(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
		Level().SetDemoPlaySpeed(Level().GetDemoPlaySpeed() * 2);
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Increases demo play speed"); };
};

class CCC_DivDemoPlaySpeed : public IConsole_Command {
public:
					CCC_DivDemoPlaySpeed(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute(LPCSTR args) 
	{
		if (!Level().IsDemoPlayStarted())
		{
			Msg("! Demo play not started.");
			return;
		}
		float curr_demo_speed = Level().GetDemoPlaySpeed();
		if (curr_demo_speed <= 0.2f)
		{
			Msg("! Can't decrease demo speed");
			return;
		}
		Level().SetDemoPlaySpeed(curr_demo_speed / 2);
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Decreases demo play speed"); };
};

class CCC_ScreenshotAllPlayers : public IConsole_Command {
public:
	CCC_ScreenshotAllPlayers (LPCSTR N) : IConsole_Command(N) { bEmptyArgsHandled = true; };
	virtual void	Execute		(LPCSTR args_) 
	{
		if (!g_pGameLevel || !Level().Server) return;
		struct ScreenshotMaker
		{
			xrClientData* admin_client;
			void operator()(IClient* C)
			{
				Level().Server->MakeScreenshot(admin_client->ID, C->ID);
			}
		};
		ScreenshotMaker tmp_functor;
		tmp_functor.admin_client = exclude_command_initiator(args_);
		if (!tmp_functor.admin_client)
		{
			Msg("! ERROR: only radmin can make screenshots (use \"ra login\")");
			return;
		}
		Level().Server->ForEachClientDo(tmp_functor);
	}
	virtual void	Info		(TInfo& I)
	{
		xr_strcpy(I, 
			"Make screenshot of each player in the game. Format: \"screenshot_all");
	}

}; //class CCC_ScreenshotAllPlayers

class CCC_ConfigsDumpAll : public IConsole_Command {
public:
	CCC_ConfigsDumpAll (LPCSTR N) : IConsole_Command(N) { bEmptyArgsHandled = true; };
	virtual void	Execute		(LPCSTR args_) 
	{
		if (!g_pGameLevel || !Level().Server) return;
		struct ConfigDumper
		{
			xrClientData* admin_client;
			void operator()(IClient* C)
			{
				Level().Server->MakeConfigDump(admin_client->ID, C->ID);
			}
		};
		ConfigDumper tmp_functor;
		tmp_functor.admin_client = exclude_command_initiator(args_);
		if (!tmp_functor.admin_client)
		{
			Msg("! ERROR: only radmin can make config dumps (use \"ra login\")");
			return;
		}
		Level().Server->ForEachClientDo(tmp_functor);
	}
	virtual void	Info		(TInfo& I)
	{
		xr_strcpy(I, 
			"Make config dump of each player in the game. Format: \"config_dump_all");
	}
}; //class CCC_ConfigsDumpAll


#ifdef DEBUG

class CCC_DbgMakeScreenshot : public IConsole_Command
{
public:
	CCC_DbgMakeScreenshot(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void Execute(LPCSTR args) {
		if (!g_pGameLevel || !Level().Server)
			return;
		ClientID server_id(Level().Server->GetServerClient()->ID);
		Level().Server->MakeScreenshot(server_id, server_id);
	}
}; //CCC_DbgMakeScreenshot

#endif //#ifdef DEBUG

class CCC_Name : public IConsole_Command
{
public:
	CCC_Name(LPCSTR N) : IConsole_Command(N)  { bLowerCaseArgs = false;	bEmptyArgsHandled = false; };
	virtual void	Status	(TStatus& S)
	{ 
		S[0]=0;
		if( IsGameTypeSingle() )									return;
		if (!(&Level()))											return;
		if (!(&Game()))												return;
		game_PlayerState* tmp_player = Game().local_player;
		if (!tmp_player)											return;
	}

	virtual void	Save	(IWriter *F)	{}

	virtual void Execute(LPCSTR args) 
	{
		if (IsGameTypeSingle())		return;
		if (!(&Level()))			return;
		if (!(&Game()))				return;

		game_PlayerState* tmp_player = Game().local_player;
		if (!tmp_player)			return;

		if (!xr_strlen(args)) return;
		if (strchr(args, '/'))
		{
			Msg("!  '/' is not allowed in names!");
			return;
		}
		string4096 NewName = "";
	
		NET_Packet				P;
		Game().u_EventGen		(P,GE_GAME_EVENT,Game().local_player->GameID);
		P.w_u16					(GAME_EVENT_PLAYER_NAME);
		P.w_stringZ				(NewName);
		Game().u_EventSend		(P);
	}

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"player name"); }
};

class CCC_ChangeLevelGameType : public IConsole_Command {
public:
					CCC_ChangeLevelGameType	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute					(LPCSTR args) 
	{
		if (!OnServer())	return;
		if (!xr_strlen(args))
		{
			Msg("Changing level, version and game type. Arguments: <level name> <level version> <game type>");
			return;
		}

		string256		LevelName;	
		LevelName[0]	=0;
		string256		LevelVersion;
		LevelVersion[0]	= 0;
		string256		GameType;	
		GameType[0]		=0;
		
		sscanf_s		(args,"%255s %255s %255s",
			LevelName, sizeof(LevelName),
			LevelVersion, sizeof(LevelVersion),
			GameType, sizeof(GameType)
		);

		EGameIDs GameTypeID = ParseStringToGameType(GameType);
		if(GameTypeID==eGameIDNoGame)
		{
			Msg ("! Unknown gametype - %s", GameType);
			return;
		};
		//-----------------------------------------

		const SGameTypeMaps& M		= gMapListHelper.GetMapListFor(GameTypeID);
		u32 cnt						= M.m_map_names.size();
		bool bMapFound				= false;
		for(u32 i=0; i<cnt; ++i)
		{
			const MPLevelDesc& itm = M.m_map_names[i];
			if (!xr_strcmp(itm.map_name.c_str(), LevelName) &&
				!xr_strcmp(itm.map_ver.c_str(), LevelVersion))
			{
				bMapFound = true;
				break;
			}
		}
		if (!bMapFound)
		{
			Msg("! Level [%s][%s] not found for [%s]!", LevelName, LevelVersion, GameType);
			return;
		}

		NET_Packet			P;
		P.w_begin			(M_CHANGE_LEVEL_GAME);
		P.w_stringZ			(LevelName);
		P.w_stringZ			(LevelVersion);
		P.w_stringZ			(GameType);
		Level().Send		(P);
	};

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"Changing level, version and game type. Arguments: <level name> <level version> <game type>"); }
};

class CCC_ChangeGameType : public CCC_ChangeLevelGameType {
public:
					CCC_ChangeGameType	(LPCSTR N) : CCC_ChangeLevelGameType(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute				(LPCSTR args) 
	{

		if (!OnServer())	return;

		//string256			GameType;	
		//GameType[0]			=0;
		//sscanf				(args,"%s", GameType);

		string1024			argsNew;
		xr_sprintf				(argsNew, "%s %s %s", 
			Level().name().c_str(), 
			Level().version().c_str(),
			args);

		CCC_ChangeLevelGameType::Execute((LPCSTR)argsNew);
	};

	virtual void	Info	(TInfo& I)
	{
		xr_strcpy(I,"Changing Game Type : <dm>,<tdm>,<ah>,<cta>");
	}

	virtual void	fill_tips(vecTips& tips, u32 mode)
	{
		if ( g_pGameLevel && Level().Server && OnServer() && Level().Server->game )
		{
			EGameIDs type = Level().Server->game->Type();
			TStatus  str;
			xr_sprintf( str, sizeof(str), "%s  (current game type)  [dm,tdm,ah,cta]", GameTypeToString( type, true ) );
			tips.push_back( str );
		}
		IConsole_Command::fill_tips( tips, mode );
	}	

};

class CCC_ChangeLevel : public CCC_ChangeLevelGameType {
public:
					CCC_ChangeLevel	(LPCSTR N) : CCC_ChangeLevelGameType(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (!OnServer())	return;
		if (!xr_strlen(args))
		{
			Msg("Changing Game Type. Arguments: <level name> <level version>");
			return;
		}

		string256		LevelName;
		string256		LevelVersion;
		LevelName[0]	=	0;
		LevelVersion[0] =	0;
		sscanf_s		(args,"%255s %255s",
			LevelName, sizeof(LevelName),
			LevelVersion, sizeof(LevelVersion)
		);

		string1024		argsNew;
		xr_sprintf		(argsNew, "%s %s %s", LevelName, LevelVersion, Level().Server->game->type_name());

		CCC_ChangeLevelGameType::Execute((LPCSTR)argsNew);
	};

	virtual void	Info	(TInfo& I){	xr_strcpy(I,"Changing Game Type. Arguments: <level name> <level version>"); }
};

class CCC_AddMap : public IConsole_Command {
public:
	CCC_AddMap(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void Execute(LPCSTR args) 
	{
		if (!g_pGameLevel || !Level().Server || !Level().Server->game) return;

		string512	MapName, MapVer;
		LPCSTR c	= strstr(args, "/ver=");
		if(!c)
			strncpy_s	(MapName, sizeof(MapName), args, sizeof(MapName)-1 );
		else
		{
			strncpy_s	(MapName, sizeof(MapName), args, c-args);
			xr_strcpy	(MapVer, sizeof(MapVer), c+5);
		}

		Level().Server->game->MapRotation_AddMap(MapName, MapVer);
	};

	virtual void	Info	(TInfo& I)		
	{
		xr_strcpy(I,"Adds map to map rotation list"); 
	}
};

class CCC_ListMaps : public IConsole_Command {
public:
					CCC_ListMaps	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (!g_pGameLevel || !Level().Server || !Level().Server->game) return;
		Level().Server->game->MapRotation_ListMaps();
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"List maps in map rotation list"); }
};

class CCC_NextMap : public IConsole_Command {
public:
					CCC_NextMap		(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute			(LPCSTR args) 
	{
		if (!OnServer())	return;

		Level().Server->game->OnNextMap();
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Switch to Next Map in map rotation list"); }
};

class CCC_PrevMap : public IConsole_Command {
public:
	CCC_PrevMap(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void Execute(LPCSTR args) 
	{
		if (!OnServer())	return;

		Level().Server->game->OnPrevMap();
	};

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"Switch to Previous Map in map rotation list"); }
};

class CCC_AnomalySet : public IConsole_Command {
public:
	CCC_AnomalySet(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void Execute(LPCSTR args) 
	{
		if (!OnServer())		return;
	};

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"Activating pointed Anomaly set"); }
};

class CCC_Vote_Yes : public IConsole_Command {
public:
					CCC_Vote_Yes(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute(LPCSTR args) 
	{
		if (IsGameTypeSingle())
		{
			Msg("! Only for multiplayer games!");
			return;
		}

		if (Game().Phase() != GAME_PHASE_INPROGRESS)
		{
			Msg("! Voting is allowed only when game is in progress!");
			return;
		};
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Vote Yes"); };
};

class CCC_Vote_No : public IConsole_Command {
public:
					CCC_Vote_No	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute		(LPCSTR args) 
	{
		if (IsGameTypeSingle())
		{
			Msg("! Only for multiplayer games!");
			return;
		}

		if (Game().Phase() != GAME_PHASE_INPROGRESS)
		{
			Msg("! Voting is allowed only when game is in progress!");
			return;
		};
	};

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"Vote No"); };
};

class CCC_StartTimeEnvironment: public IConsole_Command {
public:
					CCC_StartTimeEnvironment	(LPCSTR N) : IConsole_Command(N) {};
	virtual void	Execute						(LPCSTR args)
	{
		u32 hours = 0, mins = 0;
		
		sscanf				(args,"%d:%d", &hours, &mins);
		u64 NewTime			= generate_time	(1,1,1,hours,mins,0,0);

		if (!g_pGameLevel)
			return;

		if (!Level().Server)
			return;

		if (!Level().Server->game)
			return;

		float eFactor = Level().Server->game->GetEnvironmentGameTimeFactor();
		Level().Server->game->SetEnvironmentGameTimeFactor(NewTime,eFactor);
		Level().Server->game->SetGameTimeFactor(NewTime,g_fTimeFactor);
	}
};
class CCC_SetWeather : public IConsole_Command {
public:
					CCC_SetWeather	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute			(LPCSTR weather_name) 
	{
		if (!g_pGamePersistent) return;
		if (!OnServer())		return;

		if ( weather_name && weather_name[0] )
		{
			g_pGamePersistent->Environment().SetWeather(weather_name);		
		}
	};

	virtual void	Info	(TInfo& I){xr_strcpy(I,"Set new weather"); }
};

class CCC_SaveStatistic : public IConsole_Command {
public:
					CCC_SaveStatistic	(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute				(LPCSTR args) {
		if (!Level().Server)
			return;
	}
	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"saving statistic data"); }
};

class CCC_ReturnToBase: public IConsole_Command {
public:
					CCC_ReturnToBase(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute(LPCSTR args) 
	{
		if (!OnServer())						return;
		if (GameID() != eGameIDArtefactHunt)		return;
	}
};

class CCC_StartTeamMoney : public IConsole_Command {
public:
					CCC_StartTeamMoney(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute(LPCSTR args) 
	{
		if (!OnServer())	return;

		string512			Team = "";
		s32 TeamMoney		= 0;
		sscanf				(args,"%s %i", Team, &TeamMoney);

		if (!Team[0])
		{
			Msg("- --------------------");
			Msg("Teams start money:");
			Msg("- --------------------");
			return;
		}else
		{
			u32 TeamID			= 0;
			s32 TeamStartMoney	= 0;
			sscanf				(args,"%i %i", &TeamID, &TeamStartMoney);
		}
	};

	virtual void	Info	(TInfo& I)	{xr_strcpy(I,"Set Start Team Money");}
};

class CCC_RadminCmd: public IConsole_Command
{
public:
	CCC_RadminCmd(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void Execute(LPCSTR arguments)
	{
		if ( IsGameTypeSingle() || xr_strlen(arguments) >= 512 )
		{
			return;
		}

		if(strstr(arguments,"login")==arguments)
		{
			string512			user;
			string512			pass;
			if(2==sscanf		(arguments+xr_strlen("login")+1, "%s %s", user, pass))
			{
				NET_Packet		P;			
				P.w_begin		(M_REMOTE_CONTROL_AUTH);
				P.w_stringZ		(user);
				P.w_stringZ		(pass);

				Level().Send(P,net_flags(TRUE,TRUE));
			}else
				Msg("2 args(user pass) needed");
		}
		else
		if(strstr(arguments,"logout")==arguments)
		{
			NET_Packet		P;			
			P.w_begin		(M_REMOTE_CONTROL_AUTH);
			P.w_stringZ		("logoff");

			Level().Send(P,net_flags(TRUE,TRUE));
		}//logoff
		else
		{
			NET_Packet		P;			
			P.w_begin		(M_REMOTE_CONTROL_CMD);
			P.w_stringZ		(arguments);

			Level().Send(P,net_flags(TRUE,TRUE));
		}
	}
	virtual void	Save	(IWriter *F)	{};
};

class CCC_SwapTeams : public IConsole_Command {
public:
					CCC_SwapTeams(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = true; };
	virtual void	Execute(LPCSTR args) {
		if (!OnServer()) return;
		if(Level().Server && Level().Server->game) 
		{
			Level().Server->game->round_end_reason = eRoundEnd_GameRestartedFast;
			Level().Server->game->OnRoundEnd();
		}
	}
	virtual void	Info	(TInfo& I){xr_strcpy(I,"swap teams for artefacthunt game"); }
};

class CCC_SvChat : public IConsole_Command {
public:
					CCC_SvChat(LPCSTR N) : IConsole_Command(N)  { bEmptyArgsHandled = false; };
	virtual void	Execute(LPCSTR args) {
		if (!OnServer())	return;
	}
};

void register_mp_console_commands()
{
	CMD1(CCC_Restart,				"g_restart"				);
	CMD1(CCC_RestartFast,			"g_restart_fast"		);
	CMD1(CCC_Kill,					"g_kill"				);

	// Net Interpolation
	CMD4(CCC_Float,					"net_cl_interpolation",		&g_cl_lvInterp,				-1,1);
	CMD4(CCC_Integer,				"net_cl_icurvetype",		&g_cl_InterpolationType,	0, 2)	;
	CMD4(CCC_Integer,				"net_cl_icurvesize",		(int*)&g_cl_InterpolationMaxPoints,	0, 2000)	;
	
	CMD1(CCC_Net_CL_Resync,			"net_cl_resync" );
	CMD1(CCC_Net_CL_ClearStats,		"net_cl_clearstats" );
	CMD1(CCC_Net_SV_ClearStats,		"net_sv_clearstats" );

	// Network
#ifdef DEBUG
	CMD4(CCC_Integer,	"net_cl_update_rate",	&psNET_ClientUpdate,20,		100				);
	CMD4(CCC_Integer,	"net_cl_pending_lim",	&psNET_ClientPending,0,		10				);
#endif
	CMD4(CCC_Integer,	"net_sv_update_rate",	&psNET_ServerUpdate,1,		100				);
	CMD4(CCC_Integer,	"net_sv_pending_lim",	&psNET_ServerPending,0,		10				);
	CMD4(CCC_Integer,	"net_sv_gpmode",	    &psNET_GuaranteedPacketMode,0, 2)	;
	CMD3(CCC_Mask,		"net_sv_log_data",		&psNET_Flags,		NETFLAG_LOG_SV_PACKETS	);
	CMD3(CCC_Mask,		"net_cl_log_data",		&psNET_Flags,		NETFLAG_LOG_CL_PACKETS	);
#ifdef DEBUG
	CMD3(CCC_Mask,		"net_dump_size",		&psNET_Flags,		NETFLAG_DBG_DUMPSIZE	);
	CMD1(CCC_Dbg_NumObjects,"net_dbg_objects"				);
#endif // DEBUG
	CMD4(CCC_Integer,	"g_eventdelay",			&g_dwEventDelay,	0,	1000);

	CMD1(CCC_MakeScreenshot,			"make_screenshot"			);
	CMD1(CCC_MakeConfigDump,			"make_config_dump"			);
	CMD1(CCC_ScreenshotAllPlayers,		"screenshot_all"			);
	CMD1(CCC_ConfigsDumpAll,			"config_dump_all"			);


	CMD1(CCC_SetDemoPlaySpeed,			"mpdemoplay_speed_set"		);
	CMD1(CCC_DemoPlayPauseOn,			"mpdemoplay_pause_on"		);
	CMD1(CCC_DemoPlayCancelPauseOn,		"mpdemoplay_cancel_pause_on");
	CMD1(CCC_DemoPlayRewindUntil,		"mpdemoplay_rewind_until"	);
	CMD1(CCC_DemoPlayStopRewind,		"mpdemoplay_stop_rewind"	);
	CMD1(CCC_DemoPlayRestart,			"mpdemoplay_restart"		);
	CMD1(CCC_MulDemoPlaySpeed,			"mpdemoplay_mulspeed"		);
	CMD1(CCC_DivDemoPlaySpeed,			"mpdemoplay_divspeed"		);
	
#ifdef DEBUG
	CMD1(CCC_DbgMakeScreenshot,			"dbg_make_screenshot"		);
#endif
	CMD1(CCC_ChangeGameType,		"sv_changegametype"			);
	CMD1(CCC_ChangeLevel,			"sv_changelevel"			);
	CMD1(CCC_ChangeLevelGameType,	"sv_changelevelgametype"	);	

	CMD1(CCC_AddMap,		"sv_addmap"				);	
	CMD1(CCC_ListMaps,		"sv_listmaps"				);	
	CMD1(CCC_NextMap,		"sv_nextmap"				);	
	CMD1(CCC_PrevMap,		"sv_prevmap"				);
	CMD1(CCC_AnomalySet,	"sv_nextanomalyset"			);

	CMD1(CCC_Vote_Yes,		"cl_voteyes"				);
	CMD1(CCC_Vote_No,		"cl_voteno"				);

	CMD1(CCC_StartTimeEnvironment,	"sv_setenvtime");

	CMD1(CCC_SetWeather,	"sv_setweather"			);

	CMD4(CCC_Integer,		"cl_cod_pickup_mode",	&g_b_COD_PickUpMode,	0, 1)	;

	CMD4(CCC_Integer,		"sv_remove_weapon",		&g_iWeaponRemove, -1, 1);
	CMD4(CCC_Integer,		"sv_remove_corpse",		&g_iCorpseRemove, -1, 1);

	CMD4(CCC_Integer,		"sv_statistic_collect", &g_bCollectStatisticData, 0, 1);
	CMD1(CCC_SaveStatistic,	"sv_statistic_save");

	CMD4(CCC_Integer,		"net_dbg_dump_update_write",	&g_Dump_Update_Write, 0, 1);
	CMD4(CCC_Integer,		"net_dbg_dump_update_read",	&g_Dump_Update_Read, 0, 1);

	CMD1(CCC_ReturnToBase,	"sv_return_to_base");

	CMD1(CCC_StartTeamMoney,"sv_startteammoney"		);		

	CMD4(CCC_Integer,		"sv_client_reconnect_time",		(int*)&g_sv_Client_Reconnect_Time, 0, 60);

	CMD4(CCC_Integer,		"cl_mpdemosave"				,	(int*)&g_cl_save_demo, 0, 1);

	CMD1(CCC_SwapTeams,		"g_swapteams"				);

	CMD1(CCC_RadminCmd,		"ra");
	CMD1(CCC_Name,			"name");
	CMD1(CCC_SvChat,		"chat");

}
