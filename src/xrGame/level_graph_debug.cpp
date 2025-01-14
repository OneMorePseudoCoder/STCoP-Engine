////////////////////////////////////////////////////////////////////////////
//	Module 		: level_graph_debug.cpp
//	Created 	: 02.10.2001
//  Modified 	: 11.11.2003
//	Author		: Oles Shihkovtsov, Dmitriy Iassenev
//	Description : Level graph debug functions
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef DEBUG
#ifndef AI_COMPILER

#include "level_graph.h"
#include "level.h"
#include "game_base_space.h"
#include "xrserver_objects_alife_monsters.h"
#include "alife_simulator.h"
#include "alife_graph_registry.h"
#include "alife_object_registry.h"
#include "alife_human_brain.h"
#include "alife_monster_movement_manager.h"
#include "alife_monster_detail_path_manager.h"
#include "ui_base.h"

#include "debug_renderer.h"

void CLevelGraph::setup_current_level	(const int &level_id)
{
	if (m_current_level_id == level_id)
		return;

	m_current_actual		= false;
	m_current_level_id		= level_id;
}

void CLevelGraph::render	()
{
	if (psAI_Flags.test(aiDrawGameGraph)) {
//		if (psHUD_Flags.test(HUD_DRAW))
			draw_game_graph	();
	}

	if (!bDebug && !psAI_Flags.test(aiMotion))
		return;

	if (bDebug && psAI_Flags.test(aiDebug))
		draw_nodes			();

	draw_restrictions		();

	if (psAI_Flags.test(aiCover))
		draw_covers			();

	if (psAI_Flags.test(aiMotion))
		draw_objects		();

#ifdef DEBUG
	draw_debug_node			();
#endif
}

void modify							(const int &vertex_id, Fbox &bounding_box)
{
	const CGameGraph		&graph = ai().game_graph();
	bounding_box.modify		(graph.vertex(vertex_id)->game_point());

	CGameGraph::const_iterator	I,E;
	graph.begin				(vertex_id,I,E);
	for ( ; I != E; ++I)
		bounding_box.modify	(graph.vertex(graph.value(vertex_id,I))->game_point());
}

void CLevelGraph::update_current_info	()
{
	m_current_actual		= true;

	Fbox					bounding_box;
	bounding_box.invalidate	();

	bool					found = false;
	bool					all = (m_current_level_id == -1);
	const CGameGraph		&graph = ai().game_graph();
	for (int i=0, n = (int)graph.header().vertex_count(); i<n; ++i) {
		if (!all) {
			if (graph.vertex(i)->level_id() != m_current_level_id) {
				if (found)
					break;
				continue;
			}
			found			= true;
		}

		modify				(i,bounding_box);
	}

	bounding_box.getcenter	(m_current_center);
	bounding_box.getradius	(m_current_radius);
}

Fvector CLevelGraph::convert_position	(const Fvector &position)
{
	Fvector					result = position;
	result.sub				(m_current_center,position);
	result.x				*= 5.f/m_current_radius.x;
	result.y				*= 1.f/m_current_radius.y;
	result.z				*= 5.f/m_current_radius.z;

	result.mul				(.5f);
	result.add				(Level().CurrentEntity()->Position());
	result.y				+= 4.5f;

	return					(result);
}

void CLevelGraph::draw_edge			(const int &vertex_id0, const int &vertex_id1)
{
	const u8				*vt0 = ai().game_graph().vertex(vertex_id0)->vertex_type();
	const u8				*vt1 = ai().game_graph().vertex(vertex_id1)->vertex_type();
	
	float				radius = 0.005f;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
		radius = 1.f;
	u32						vertex_color0 = D3DCOLOR_XRGB(0,255,255);
	if (vt0[3] == 0)
		vertex_color0 = D3DCOLOR_XRGB(255,0,255);
	u32						vertex_color1 = D3DCOLOR_XRGB(0,255,255);
	if (vt1[3] == 0)
		vertex_color1 = D3DCOLOR_XRGB(255,0,255);
	const u32				edge_color = D3DCOLOR_XRGB(0,255,0);
	
	const CGameGraph		&graph = ai().game_graph();
	Fvector					position0;
	Fvector					position1;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
	{
		position0 = graph.vertex(vertex_id0)->level_point();
		position1 = graph.vertex(vertex_id1)->level_point();
	}
	else
	{
		position0 = convert_position(graph.vertex(vertex_id0)->game_point());
		position1 = convert_position(graph.vertex(vertex_id1)->game_point());
	}

	CDebugRenderer			&render = Level().debug_renderer();
	render.draw_aabb		(position0,radius,radius,radius,vertex_color0);
	render.draw_aabb		(position1,radius,radius,radius,vertex_color1);
	render.draw_line		(Fidentity,position0,position1,edge_color);
//	RCache.dbg_DrawAABB		(position0,radius,radius,radius,vertex_color);
//	RCache.dbg_DrawAABB		(position1,radius,radius,radius,vertex_color);
//	RCache.dbg_DrawLINE		(Fidentity,position0,position1,edge_color);
}

void CLevelGraph::draw_vertex		(const int &vertex_id)
{
	CGameGraph::const_iterator	I,E;
	const CGameGraph			&graph = ai().game_graph();
	graph.begin					(vertex_id,I,E);
	for ( ; I != E; ++I) {
		int						neighbour_id = graph.value(vertex_id,I);
		if (neighbour_id < vertex_id)
			draw_edge			(vertex_id,neighbour_id);
	}
}

void CLevelGraph::draw_stalkers		(const int &vertex_id)
{
	if (!ai().get_alife())
		return;
	float				radius = .0105f;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
		radius = 1.f;
	const u32					color = D3DCOLOR_XRGB(255,0,0);
	const CGameGraph			&graph = ai().game_graph();
	CGameFont					&font = *UI().Font().pFontDI;

	Fvector						position;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
		position = graph.vertex(vertex_id)->level_point();
	else
		position = convert_position(graph.vertex(vertex_id)->game_point());

	font.SetColor				(D3DCOLOR_XRGB(255,255,0));

	bool						show_text = true;
	for (;;) {
		Fvector4				temp;
		Device.mFullTransform.transform (temp,position);
		font.OutSetI			(temp.x,-temp.y);
		font.SetHeightI			(.05f/_sqrt(temp.w));
		
		if (temp.z < 0.f) {
			show_text			= false;
			break;
		}

		if (temp.w < 0.f) {
			show_text			= false;
			break;
		}

		if (temp.x < -1.f) {
			show_text			= false;
			break;
		}
		
		if (temp.x > 1.f) {
			show_text			= false;
			break;
		}

		if (temp.y < -1.f) {
			show_text			= false;
			break;
		}
		
		if (temp.x > 1.f) {
			show_text			= false;
			break;
		}

		break;
	}

	typedef CALifeGraphRegistry::OBJECT_REGISTRY	OBJECT_REGISTRY;
	typedef OBJECT_REGISTRY::_const_iterator		const_iterator;
	typedef CALifeMonsterDetailPathManager::PATH	PATH;
	const OBJECT_REGISTRY		&objects = ai().alife().graph().objects()[vertex_id].objects();

	CDebugRenderer				&render = Level().debug_renderer();
	if (show_text) {
		bool					first_time = true;
		const_iterator			I = objects.objects().begin();
		const_iterator			E = objects.objects().end();
		for (; I != E; ++I) {
			CSE_ALifeDynamicObject	*object = (*I).second;
			CSE_ALifeHumanStalker	*stalker = smart_cast<CSE_ALifeHumanStalker*>(object);
			if (!stalker)
				continue;

			const PATH			&path = stalker->brain().movement().detail().path();
			const float			&walked_distance = (path.size() < 2) ? 0.f : stalker->brain().movement().detail().walked_distance();
//			font.OutNext		("%s",stalker->name_replace());

			if ((path.size() >= 2) && !fis_zero(walked_distance))
				continue;

			if (!first_time)
				continue;

			Fvector						position;
			if (psAI_Flags.test(aiDrawGameGraphRealPos))
				position = graph.vertex(stalker->m_tGraphID)->level_point();
			else
				position = convert_position(graph.vertex(stalker->m_tGraphID)->game_point());
			
			render.draw_aabb	(position,radius,radius,radius,color);
			first_time			= false;
			continue;
		}
	}

	const_iterator				I = objects.objects().begin();
	const_iterator				E = objects.objects().end();
	for (; I != E; ++I) {
		CSE_ALifeDynamicObject	*object = (*I).second;
		CSE_ALifeHumanStalker	*stalker = smart_cast<CSE_ALifeHumanStalker*>(object);
		if (!stalker)
			continue;

		const PATH				&path = stalker->brain().movement().detail().path();
		if (path.size() < 2)
			continue;

		u32						game_vertex_id0 = stalker->m_tGraphID;
		u32						game_vertex_id1 = path[path.size() - 2];
		const float				&walked_distance = stalker->brain().movement().detail().walked_distance();

		if (fis_zero(walked_distance))
			continue;



		Fvector					position0;
		Fvector					position1;
		float					distance;
		if (psAI_Flags.test(aiDrawGameGraphRealPos))
		{
			position0 = graph.vertex(game_vertex_id0)->level_point();
			position1 = graph.vertex(game_vertex_id1)->level_point();
			distance = position0.distance_to(position1);
		}
		else
		{
			position0 = convert_position(graph.vertex(game_vertex_id0)->game_point());
			position1 = convert_position(graph.vertex(game_vertex_id1)->game_point());
			distance = graph.vertex(game_vertex_id0)->game_point().distance_to(graph.vertex(game_vertex_id1)->game_point());
		}

		Fvector					direction = Fvector().sub(position1,position0);
		float					magnitude = direction.magnitude();
		direction.normalize		();
		direction.mul			(magnitude*walked_distance/distance);
		direction.add			(position0);
		render.draw_aabb		(direction,radius,radius,radius,color);

		Fvector4				temp;
		Device.mFullTransform.transform (temp,direction);
		
		if (temp.z < 0.f)
			continue;

		if (temp.w < 0.f)
			continue;

		if (temp.x < -1.f)
			continue;
		
		if (temp.x > 1.f)
			continue;

		if (temp.y < -1.f)
			continue;
		
		if (temp.x > 1.f)
			continue;

		font.SetHeightI			(.05f/_sqrt(temp.w));
	}
}

void CLevelGraph::draw_objects		(const int &vertex_id)
{
	if (!ai().get_alife())
		return;

	float				radius = .0105f;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
		radius = 1.f;
	const u32					color = D3DCOLOR_XRGB(255,0,0);
	const CGameGraph			&graph = ai().game_graph();
	CGameFont					&font = *UI().Font().pFontDI;
	Fvector						position;
	if (psAI_Flags.test(aiDrawGameGraphRealPos))
		position = graph.vertex(vertex_id)->level_point();
	else
		position = convert_position(graph.vertex(vertex_id)->game_point());

	font.SetColor				(D3DCOLOR_XRGB(255,255,0));

	bool						show_text = true;
	for (;;) {
		Fvector4				temp;
		Device.mFullTransform.transform (temp,position);
		font.OutSetI			(temp.x,-temp.y);
		font.SetHeightI			(.05f/_sqrt(temp.w));
		
		if (temp.z < 0.f) {
			show_text			= false;
			break;
		}

		if (temp.w < 0.f) {
			show_text			= false;
			break;
		}

		if (temp.x < -1.f) {
			show_text			= false;
			break;
		}
		
		if (temp.x > 1.f) {
			show_text			= false;
			break;
		}

		if (temp.y < -1.f) {
			show_text			= false;
			break;
		}
		
		if (temp.x > 1.f) {
			show_text			= false;
			break;
		}

		break;
	}

	typedef CALifeGraphRegistry::OBJECT_REGISTRY	OBJECT_REGISTRY;
	typedef OBJECT_REGISTRY::_const_iterator		const_iterator;
	typedef CALifeMonsterDetailPathManager::PATH	PATH;
	const OBJECT_REGISTRY		&objects = ai().alife().graph().objects()[vertex_id].objects();

	CDebugRenderer				&render = Level().debug_renderer();
	if (show_text) {
		bool					first_time = true;
		const_iterator			I = objects.objects().begin();
		const_iterator			E = objects.objects().end();
		for (; I != E; ++I) {
			CSE_ALifeDynamicObject	*object = (*I).second;
			CSE_ALifeMonsterAbstract*monster = smart_cast<CSE_ALifeMonsterAbstract*>(object);
			if (!monster)
				continue;

			const PATH			&path = monster->brain().movement().detail().path();
			const float			&walked_distance = (path.size() < 2) ? 0.f : monster->brain().movement().detail().walked_distance();

			if ((path.size() >= 2) && !fis_zero(walked_distance))
				continue;

			if (!first_time)
				continue;

			Fvector						position;
			if (psAI_Flags.test(aiDrawGameGraphRealPos))
				position = graph.vertex(monster->m_tGraphID)->level_point();
			else
				position = convert_position(graph.vertex(monster->m_tGraphID)->game_point());

			render.draw_aabb	(position,radius,radius,radius,color);
			first_time			= false;
			continue;
		}
	}

	const_iterator				I = objects.objects().begin();
	const_iterator				E = objects.objects().end();
	for (; I != E; ++I) {
		CSE_ALifeDynamicObject	*object = (*I).second;
		CSE_ALifeMonsterAbstract*monster = smart_cast<CSE_ALifeMonsterAbstract*>(object);
		if (!monster)
			continue;

		const PATH				&path = monster->brain().movement().detail().path();
		if (path.size() < 2)
			continue;

		u32						game_vertex_id0 = monster->m_tGraphID;
		u32						game_vertex_id1 = path[path.size() - 2];
		const float				&walked_distance = monster->brain().movement().detail().walked_distance();

		if (fis_zero(walked_distance))
			continue;

		Fvector					position0;
		Fvector					position1;
		float					distance;
		if (psAI_Flags.test(aiDrawGameGraphRealPos))
		{
			position0 = graph.vertex(game_vertex_id0)->level_point();
			position1 = graph.vertex(game_vertex_id1)->level_point();
			distance = position0.distance_to(position1);
		}
		else
		{
			position0 = convert_position(graph.vertex(game_vertex_id0)->game_point());
			position1 = convert_position(graph.vertex(game_vertex_id1)->game_point());
			distance = graph.vertex(game_vertex_id0)->game_point().distance_to(graph.vertex(game_vertex_id1)->game_point());
		}

		Fvector					direction = Fvector().sub(position1,position0);
		float					magnitude = direction.magnitude();
		direction.normalize		();
		direction.mul			(magnitude*walked_distance/distance);
		direction.add			(position0);
		render.draw_aabb		(direction,radius,radius,radius,color);

		Fvector4				temp;
		Device.mFullTransform.transform (temp,direction);
		
		if (temp.z < 0.f)
			continue;

		if (temp.w < 0.f)
			continue;

		if (temp.x < -1.f)
			continue;
		
		if (temp.x > 1.f)
			continue;

		if (temp.y < -1.f)
			continue;
		
		if (temp.x > 1.f)
			continue;

		font.SetHeightI			(.05f/_sqrt(temp.w));
	}
}

void CLevelGraph::draw_game_graph	()
{
	if (!Level().CurrentEntity())
		return;

	const Fmatrix			&xform = Level().CurrentEntity()->XFORM();
	Fvector					center = Fvector().set(0.f,5.f,0.f);
	Fvector					bounds = Fvector().set(3.f,0.f,3.f);

	// draw back plane
	Fvector					vertices[4];
	xform.transform_tiny	(vertices[0], Fvector().set(center.x - bounds.x, center.y + bounds.y, center.z + bounds.z));
	xform.transform_tiny	(vertices[1], Fvector().set(center.x + bounds.x, center.y + bounds.y, center.z + bounds.z));
	xform.transform_tiny	(vertices[2], Fvector().set(center.x - bounds.x, center.y - bounds.y, center.z - bounds.z));
	xform.transform_tiny	(vertices[3], Fvector().set(center.x + bounds.x, center.y - bounds.y, center.z - bounds.z));

	// draw vertices
	CGameGraph				&graph = ai().game_graph();
	update_current_info		();

	bool					found = false;
	bool					all = (m_current_level_id == -1);
	for (int i=0, n = (int)graph.header().vertex_count(); i<n; ++i) {
		if (!all) {
			if (graph.vertex(i)->level_id() != m_current_level_id) {
				if (found)
					break;

				continue;
			}

			found			= true;
		}
		
		draw_vertex			(i);
		
		if (psAI_Flags.test(aiDrawGameGraphStalkers))
			draw_stalkers	(i);

		if (psAI_Flags.test(aiDrawGameGraphObjects))
			draw_objects	(i);
	}
}

#endif // AI_COMPILER
#endif // DEBUG