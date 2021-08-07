#ifndef UIGAMEMP_H
#define UIGAMEMP_H

#include "UIGameCustom.h"

class CUIServerInfo;
class CUIAchivementIndicator;

class UIGameMP : public CUIGameCustom
{
	typedef CUIGameCustom inherited;
public:
					UIGameMP			();
	virtual			~UIGameMP			();
	
	void			SetServerLogo			(u8 const * data_ptr, u32 data_size);
	void			SetServerRules			(u8 const * data_ptr, u32 data_size);
	void			AddAchivment			(shared_str const & achivement_name,
											 shared_str const & color_animation,
											 u32 const width,
											 u32 const height);

	bool			IsServerInfoShown		();
	bool			ShowServerInfo			(); //shows only if it has some info ...

	virtual bool 	IR_UIOnKeyboardPress	(int dik);
	virtual bool 	IR_UIOnKeyboardRelease	(int dik);
	virtual void	SetClGame				(game_cl_GameState* g);
protected:
	CUIServerInfo*				m_pServerInfo;
	CUIAchivementIndicator*		m_pAchivementIdicator;
}; //class UIGameMP

#endif //#ifndef UIGAMEMP_H