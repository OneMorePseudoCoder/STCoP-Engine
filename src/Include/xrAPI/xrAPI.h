#ifndef	xrApi_included
#define xrApi_included
#pragma once

#ifdef XRAPI_EXPORTS
#define XRAPI_API __declspec(dllexport)
#else
#define XRAPI_API __declspec(dllimport)
#endif

#ifndef _EDITOR
class IRender_interface;
extern XRAPI_API IRender_interface*	Render;

class IRenderFactory;
extern XRAPI_API IRenderFactory*	RenderFactory;

class CDUInterface;
extern XRAPI_API CDUInterface*	DU;

struct xr_token;
extern XRAPI_API xr_token*	vid_mode_token;

class IUIRender;
extern XRAPI_API IUIRender*	UIRender;


#ifndef	_EDITOR
class CGameMtlLibrary;
extern XRAPI_API CGameMtlLibrary *			PGMLib;
#endif

class IDebugRender;
extern XRAPI_API IDebugRender*	DRender;

#else
	class	CRender;
    extern ENGINE_API CRender*	Render;

   class IRenderFactory;
    extern ENGINE_API IRenderFactory*	RenderFactory;
#endif

#endif	//	xrApi_included