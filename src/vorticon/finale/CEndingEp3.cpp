/*
 * CEndingEp3.cpp
 *
 *  Created on: 04.11.2009
 *      Author: gerstrong
 */

#include "CEndingEp3.h"
#include "../../StringUtils.h"
#include "../../sdl/CTimer.h"
#include "../../sdl/CInput.h"
#include "../../sdl/CVideoDriver.h"
#include "../../graphics/CGfxEngine.h"
#include "../../common/CMapLoader.h"
#include "../../common/Playerdefines.h"

#define SAFE_DELETE(x) if(x) { delete x; x=NULL; }

CEndingEp3::CEndingEp3(CMap *p_map, std::vector<CPlayer> &Player) :
m_Player(Player)
{
		m_Episode = 3;
		mp_Map = p_map;
		m_step = 0;
		m_starttime = g_pTimer->getTicks();
		m_timepassed = 0;
		m_mustsetup = true;
		m_mustfinishgame = false;
}

void CEndingEp3::process()
{
	m_timepassed = g_pTimer->getTicks() - m_starttime;

	switch(m_step)
	{
	case 0: HonorScene(); break;
	case 1: AwardScene(); break;
	default:
		m_mustfinishgame = true;
		break;
	}
}

void CEndingEp3::HonorScene()
{
	if(m_mustsetup)
	{
		//Initialization
		std::string path = mp_Map->m_gamepath;
		CMapLoader MapLoader(mp_Map, &m_Player[0]);
		MapLoader.load(3, 81, path);

		m_Player[0].hideplayer = false;
		m_Player[0].x = 244<<STC;
		m_Player[0].y = 104<<STC;
		m_Player[0].playframe = 0;

		mp_Map->gotoPos(32, 32);
		mp_Map->drawAll();

		m_TextBoxes.push_back(new CMessageBox(getstring("EP3_ESEQ_PAGE1"), true));
		m_TextBoxes.push_back(new CMessageBox(getstring("EP3_ESEQ_PAGE2"), true));
		m_TextBoxes.push_back(new CMessageBox(getstring("EP3_ESEQ_PAGE3"), true));
		m_TextBoxes.push_back(new CMessageBox(getstring("EP3_ESEQ_PAGE4"), true));

		int newtile = mp_Map->at(2,12);
		for(int x=0 ; x<22 ; x++) // This changes to the Oh No! Tiles to normal Stone-Tiles
		{
			mp_Map->changeTile( x, 15, newtile);
			mp_Map->changeTile( x, 16, newtile);
		}

		m_mustsetup = false;
	}

	if(!m_TextBoxes.empty())
	{
		CMessageBox *pMB = m_TextBoxes.front();

		//mp_DlgFrame->draw(g_pVideoDriver->FGLayerSurface);
		pMB->process();

		if(pMB->isFinished())
		{
			delete pMB;
			m_TextBoxes.pop_front();
		}
	}
	else
	{
		m_step++;
		m_mustsetup = true;
	}
}

void CEndingEp3::AwardScene()
{
	if(m_mustsetup)
	{
		//Initialization
		mp_Map->gotoPos(0,0);
		mp_Map->resetScrolls(); // The Scrollsurface must be (0,0) so the bitmap is correctly drawn
		mp_Map->m_animation_enabled = false; // Needed, because the other map is still loaded
		m_Player[0].hideplayer = true;
		mp_FinaleStaticScene = new CFinaleStaticScene(mp_Map->m_gamepath, "finale.ck3");

		mp_FinaleStaticScene->push_string("THE_END", 6000);

		m_mustsetup = false;
	}

	if( !mp_FinaleStaticScene->mustclose() )
	{
		mp_FinaleStaticScene->process();
	}
	else
	{
		// Shutdown code here!
		delete mp_FinaleStaticScene;
		mp_FinaleStaticScene = NULL;
		mp_Map->m_animation_enabled = true;
		m_step++;
		m_mustsetup = true;
	}
}

CEndingEp3::~CEndingEp3() {
	while(!m_TextBoxes.empty())
	{
		delete m_TextBoxes.front();
		m_TextBoxes.pop_front();
	}

	SAFE_DELETE(mp_FinaleStaticScene);
}
