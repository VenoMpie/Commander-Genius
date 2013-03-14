/*
 * CFlag.cpp
 *
 *  Created on: 30.07.2010
 *      Author: gerstrong
 */

#include "CFlag.h"
#include "sdl/sound/CSound.h"
#include "common/CBehaviorEngine.h"


namespace galaxy {    

enum FLAGACTIONS
{
A_FLAG_WAVE = 0,
A_FLAG_FLIP = 4
};
    

const Uint16 FLYING_BASEFRAME_EP4 = 174;
const Uint16 FLYING_BASEFRAME_EP5 = 173;

const Uint16 ANIMATION_TIME = 8;
const Uint16 SPEED = 64;

CFlag::CFlag(CMap *pmap, const VectorD2<Uint32> &Location,
			const VectorD2<Uint32> &Destination,
			bool newAction) :
CGalaxySpriteObject(pmap, FOE_ID, Location.x, Location.y),
m_destination(Destination),
m_baseframe(0)
{
	solid = false;
	honorPriority = false;
		
	mActionMap[A_FLAG_WAVE] = &CFlag::processWaving;
	mActionMap[A_FLAG_FLIP] = &CFlag::processFlipping;
	
	const auto episode = g_pBehaviorEngine->getEpisode();

	if(episode == 6)
	{
	    setupGalaxyObjectOnMap(0x13F4, A_FLAG_FLIP);
	}
	else if(episode == 5)
	{
	    // In Episode 5 the sign is not thrown! It just appears in the holder. Move it there!
	    moveTo(m_destination);
	    setupGalaxyObjectOnMap(0x148A, A_FLAG_WAVE);
	}
	else
	{
	    setupGalaxyObjectOnMap(0x15EE, A_FLAG_FLIP);
	}
	    
	if(!newAction)
	    moveTo(m_destination);
}

void CFlag::getTouchedBy(CSpriteObject &theObject)
{    
    if( getActionNumber(A_FLAG_FLIP) )
	return;
    
    // In case another flag is sitting in the pole, make that one non existent
    if( CFlag *flag = dynamic_cast<CFlag*>(&theObject) )
    {
	flag->exists = false;
    }
}

void CFlag::setActionForce(const size_t ActionNumber)
{
	CGalaxySpriteObject::setActionForce(ActionNumber);

	if( mActionMap.find(ActionNumber) != mActionMap.end() )
		processState = mActionMap[ActionNumber];
	else
		CGalaxySpriteObject::setActionForce(0); // This might happen, when the action-map is incomplete
}

/**
 * Main process cycle
 */
void CFlag::process()
{
    processActionRoutine();

    (this->*processState)();
}

/**
 * Called when Flag is flying to the pole
 */
void CFlag::processFlipping()
{
	if(m_Pos != m_destination)
	{
		VectorD2<int> dir = m_destination - m_Pos;
		float length = dir.GetLength();
		VectorD2<float> base_dir( dir.x/length, dir.y/length );

		if( fabs(length) < SPEED )
		{
			moveTo(m_destination);
		}
		else
		{
			moveDir(base_dir*SPEED);
		}
	}
	else
	{
	    setAction(A_FLAG_WAVE);
	    setActionSprite();
	    g_pSound->playSound( SOUND_FLAG_LAND );
	    
	    const auto episode = g_pBehaviorEngine->getEpisode();

	    if(episode == 6)	    
	    {
		VectorD2<int> tilePos = m_Pos;
		
		tilePos.y = getYDownPos();
		
		Uint32 new_tile_no = mp_Map->getPlaneDataAt(1, tilePos)+1;
		tilePos = tilePos>>CSF;
		mp_Map->setTile(tilePos.x, tilePos.y, new_tile_no, true);
	    }
	}
}

/*
 * Called when flag is in the pole. Keen 4 or 6
 */
void CFlag::processWaving()
{}

}
