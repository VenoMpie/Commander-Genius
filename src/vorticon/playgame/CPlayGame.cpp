/*
 * CPlayGame.cpp
 *
 *  Created on: 03.10.2009
 *      Author: gerstrong
 *
 * See CPlayGame.h for more information
 */

#include "CPlayGame.h"
#include "../../keen.h"
#include "../../sdl/CTimer.h"
#include "../../sdl/CVideoDriver.h"
#include "../../sdl/sound/CSound.h"
#include "../../sdl/CInput.h"
#include "../../common/CMapLoader.h"
#include "../../graphics/CGfxEngine.h"
#include "../../StringUtils.h"

#define SAFE_DELETE(x) if(x) { delete x; x = NULL; }

////
// Creation Routine
////
CPlayGame::CPlayGame( char episode, char level,
					 char numplayers, char difficulty,
					 std::string &gamepath, stOption *p_option,
					 bool finale, CSavedGame &SavedGame,
					 std::vector<stTeleporterTable> &TeleporterTable) :
m_dark(false),
mp_ObjectAI(NULL),
m_SavedGame(SavedGame),
m_TeleporterTable(TeleporterTable),
mp_HighScores(NULL)
{
	m_Episode = episode;
	m_Level = level;
	m_NumPlayers = numplayers;
	m_Difficulty = difficulty;
	m_level_command = (level==WORLD_MAP_LEVEL) ? GOTO_WORLD_MAP : START_LEVEL;
	m_NumSprites = g_pGfxEngine->getNumSprites();
	m_Gamepath = gamepath;
	m_exitgame = false;
	m_endgame = false;
	m_startgame = false;
	m_gameover = false;
	m_alldead = false;
	m_hideobjects = false;
	mp_Map = NULL;
	mp_Menu = NULL;
	mp_Finale = NULL;
	mp_gameoverbmp = NULL;
	mp_option = p_option;
	m_checkpoint_x = m_checkpoint_y = 0;
	m_checkpointset = false;

	// Create the Players
	if(m_NumPlayers == 0) m_NumPlayers = 1;
	
	for(short i=0 ; i<m_NumPlayers ; i++)
	{
		// tie puppy object so the player can interact in the level
		CObject object(m_NumPlayers);
	    object.exists = true;
		object.onscreen = true;
		object.honorPriority = true;
		object.m_type = OBJ_PLAYER;
		m_Object.push_back(object);

		// Create the new Player and also tell him to which object it belongs to...
		CPlayer Player(m_Episode, m_Level, m_Difficulty,
						i, mp_level_completed, mp_option,
						m_Object);
		Player.setDatatoZero();

		m_Player.push_back(Player);
	}

	// Create completed level list
	memset(mp_level_completed,false,MAX_LEVELS*sizeof(bool));

	// Player are tied to objects like enemies, sprites, etc., so they
	// can be drawn the same way.
	createPlayerObjects();

	m_paused = false;
	m_showPauseDialog = false;

	if(difficulty==0)
		g_pGfxEngine->Palette.setdarkness(FADE_DARKNESS_EASY);
	else if(difficulty==1)
		g_pGfxEngine->Palette.setdarkness(FADE_DARKNESS);
	else
		g_pGfxEngine->Palette.setdarkness(FADE_DARKNESS_HARD);

	if(finale) m_level_command = GOTO_FINALE;
}

// Setup all the player, when one level is started
void CPlayGame::setupPlayers()
{
	m_showKeensLeft=false;
	for (int i=0 ; i<m_NumPlayers ; i++)
	{
		if( m_Level == WORLD_MAP_LEVEL )
		{
			m_Player[i].m_playingmode = CPlayer::WORLDMAP;
			m_showKeensLeft |= ( m_Player[i].pdie == PDIE_DEAD );
		}
		else
		{
			m_Player[i].m_playingmode = CPlayer::LEVELPLAY;
			m_Player[i].playframe = PSTANDFRAME;
		}
		m_Player[i].pdie = PDIE_NODIE;
		
		// Calibrate Player to the right position, so it won't fall when level starts
		CSprite *sprite = g_pGfxEngine->Sprite[PSTANDFRAME];
		m_Player[i].w = sprite->getWidth()<<STC;
		m_Player[i].h = sprite->getHeight()<<STC;
		m_Player[i].y += (2<<CSF);
		m_Player[i].y -= m_Player[i].h;
		m_Player[i].goto_y = m_Player[i].y;
		m_Player[i].m_level = m_Level;
		
		// Set the pointers to the map and object data
		m_Player[i].setMapData(mp_Map);
		m_Player[i].setPhysics(&m_PhysicsSettings);
	}
}

bool CPlayGame::init()
{
	// Taken from the original CloneKeen. If hard-mode chosen, swap levels 5 and 9 Episode 1
	if(m_Episode == 1 && m_Difficulty > 1)
	{
		if(m_Level == 5) m_Level = 9;
		if(m_Level == 9) m_Level = 5;
	}

	// Create an empty map
	mp_Map = new CMap( g_pVideoDriver->getScrollSurface(), g_pGfxEngine->Tilemap);
	CMapLoader MapLoader( mp_Map, &m_Player[0] );
	MapLoader.m_checkpointset = m_checkpointset;
	MapLoader.mp_objvect = &m_Object;

	// load level map
	if( !mp_Map ) return false;
	if( !MapLoader.load( m_Episode, m_Level, m_Gamepath ) ) return false;

	//// If those worked fine, continue the initialization
	// draw level map
	mp_Map->drawAll();

	// Now Scroll to the position of the player and center him
	mp_Map->gotoPos( 32, 64 ); // Assure that the edges are never seen

	setupPlayers();

	// Well, all players are living because they were newly spawn.
	g_pTimer->ResetSecondsTimer();

	g_pInput->flushAll();
	
	// Initialize the AI
	mp_ObjectAI = new CObjectAI(mp_Map, m_Object, m_Player, mp_option,
								m_NumPlayers, m_Episode, m_Level,
								m_Difficulty, m_PhysicsSettings);

	// Check if Player meets the conditions to show a cutscene. This also happens, when finale of episode has reached
	verifyCutscenes();

	// When Level starts it's never dark!
	g_pGfxEngine->Palette.setdark(false);

	if(m_level_command == GOTO_FINALE)
		createFinale();
	else
		if(m_showKeensLeft)	g_pSound->playSound(SOUND_KEENSLEFT, PLAY_NOW);

	// In the case that we are in Episode 3 last Level, show Mortimer Messages
	if( m_Episode == 3 && m_Level == 16 )
	{
		m_MessageBoxes.push_back(new CMessageBox(getstring("EP3_MORTIMER")));
		m_MessageBoxes.push_back(new CMessageBox(getstring("EP3_MORTIMER2")));
		m_MessageBoxes.push_back(new CMessageBox(getstring("EP3_MORTIMER3")));
		m_MessageBoxes.push_back(new CMessageBox(getstring("EP3_MORTIMER4")));
		m_MessageBoxes.push_back(new CMessageBox(getstring("EP3_MORTIMER5")));
		g_pSound->playSound(SOUND_MORTIMER, PLAY_FORCE);
	}

	return true;
}

void CPlayGame::createPlayerObjects()
{
	// tie puppy objects so the player can interact in the level
	for (int i=0 ; i<m_NumPlayers ; i++)
	{
		CObject object(m_NumPlayers);
		m_Player[i].setDatatoZero();
		m_Player[i].m_player_number = i;
		m_Player[i].m_episode = m_Episode;
		m_Player[i].mp_levels_completed = mp_level_completed;

	    object.exists = true;
		object.onscreen = true;
		object.honorPriority = true;
		object.m_type = OBJ_PLAYER;
		m_Player[i].mp_option = mp_option;
		m_Object.push_back(object);
		m_Player[i].mp_object=&m_Object;
	}
}

////
// Process Routine
////
void CPlayGame::process()
{

	// Check for fading processes if necessary
	if(g_pGfxEngine->Palette.in_progress())
		g_pGfxEngine->Palette.applyFade();

	if(mp_HighScores) // Are we requesting Highscores
	{
		mp_HighScores->process();

		// Blit the background
		g_pVideoDriver->blitScrollSurface();

		if(mp_HighScores->destroyed())
		{
			SAFE_DELETE(mp_HighScores);
			m_endgame = true;
		}
	}
	else // No? We are in the middle of the game
	{
		// If the menu is open process it!
		if(mp_Menu)
		{
			if( mp_Menu->mustBeClosed() || mp_Menu->getExitEvent() ||
					mp_Menu->mustEndGame() || mp_Menu->mustStartGame()	)
			{
				if( mp_Menu->getExitEvent() )
					m_exitgame = true;

				if( mp_Menu->mustEndGame() )
					m_endgame = true;

				if( mp_Menu->mustStartGame() )
				{
					m_NumPlayers = mp_Menu->getNumPlayers();
					m_Difficulty = mp_Menu->getDifficulty();
					m_startgame = true;
				}

				mp_Menu->cleanup();
				SAFE_DELETE(mp_Menu);
				m_hideobjects = false;
			}
			else
			{
				mp_Menu->process();
				m_hideobjects = mp_Menu->m_hideobjects;

				if(mp_Menu->restartVideo()) // Happens when in Game resolution was changed!
				{
					mp_Menu->cleanup();
					SAFE_DELETE(mp_Menu);
					mp_Map->setSDLSurface(g_pVideoDriver->getScrollSurface());
					SDL_Rect gamerect = g_pVideoDriver->getGameResolution();
					mp_Map->m_maxscrollx = (mp_Map->m_width<<4) - gamerect.w - 36;
					mp_Map->m_maxscrolly = (mp_Map->m_height<<4) - gamerect.h - 36;
					for( int i=0 ; i<m_NumPlayers ; i++ )
						while(m_Player[i].scrollTriggers());
					mp_Map->drawAll();
				}

				if(m_SavedGame.getCommand() == CSavedGame::SAVE)
				{
					saveGameState();
				}
				else if(m_SavedGame.getCommand() == CSavedGame::LOAD)
				{
					loadGameState();
				}
			}
		}
		else if(!m_paused && m_MessageBoxes.empty()) // Game is not paused
		{
			if (!mp_Finale) // Has the game been finished?
			{
				// Perform AIs
				mp_ObjectAI->process();

				/// The following functions must be worldmap dependent
				if( m_Level == WORLD_MAP_LEVEL )
				{
					processOnWorldMap();
				}
				else
				{
					processInLevel();
				}

				// Does one of the players need to pause the game?
				for( int i=0 ; i<m_NumPlayers ; i++ )
				{
					// Did he open the status screen?
					if(m_Player[i].m_showStatusScreen)
						m_paused = true; // this is processed in processPauseDialogs!

					// Handle the Scrolling here!
					m_Player[i].scrollTriggers();
				}
			}
			else // In this case the Game has been finished, goto to the cutscenes
			{
				mp_Finale->process();

				if(mp_Finale->getHasFinished())
				{
					SAFE_DELETE(mp_Finale);

					if(!m_gameover)
					{
						mp_HighScores = new CHighScores(m_Episode, m_Gamepath, true);
						collectHighScoreInfo();
					}
				}
			}
		}
		else // In this case the game is paused
		{
			// Finally draw Dialogs like status screen, game paused, etc.
			processPauseDialogs();
		}
		// Animate the tiles of the map
		mp_Map->animateAllTiles();

		// Blit the background
		g_pVideoDriver->blitScrollSurface();

		// Draw objects to the screen
		drawObjects();

		// Check if we are in gameover mode. If yes, than show the bitmaps and block the FKeys().
		// Only confirmation button is allowes
		if(m_gameover && !mp_Finale) // game over mode
		{
			if(mp_gameoverbmp != NULL)
			{
				mp_gameoverbmp->process();

				if( g_pInput->getPressedKey(KENTER) || g_pInput->getPressedAnyCommand() )
				{
					mp_HighScores = new CHighScores(m_Episode, m_Gamepath, true);

					collectHighScoreInfo();
				}
			}
			else // Bitmap must first be created
			{
				CBitmap *pBitmap = g_pGfxEngine->getBitmap("GAMEOVER");
				g_pSound->playSound(SOUND_GAME_OVER, PLAY_NOW);
				mp_gameoverbmp = new CEGABitmap(g_pVideoDriver->getBlitSurface(), pBitmap);
				mp_gameoverbmp->setScrPos( 160-(pBitmap->getWidth()/2), 100-(pBitmap->getHeight()/2) );
			}
		}
		else // No game over
		{
			// Handle special functional keys for paused game, F1 Help, god mode, all items, etc.
			handleFKeys();
		}

		if (g_pVideoDriver->showfps)
		{
			std::string tempbuf;
			SDL_Surface *sfc = g_pVideoDriver->FGLayerSurface;
#ifdef DEBUG
			tempbuf = "FPS: " + itoa(g_pTimer->getFramesPerSec()) +
					"; x = " + itoa(m_Player[0].x) + " ; y = " + itoa(m_Player[0].y);
#else
			tempbuf = "FPS: " + itoa(g_pTimer->getFramesPerSec());
#endif
			g_pGfxEngine->Font->drawFont(sfc,tempbuf,320-(tempbuf.size()<<3)-1, LETTER_TYPE_RED);
		}

		// Open the Main Menu if ESC Key pressed and mp_Menu not opened
		if(!mp_Menu && !mp_Finale && g_pInput->getPressedCommand(IC_QUIT))
		{
			// Open the menu
			mp_Menu = new CMenu( ACTIVE, m_Gamepath, m_Episode, *mp_Map, m_SavedGame, mp_option );
			mp_Menu->init();
		}
	}
}


void CPlayGame::handleFKeys()
{
	int i;
	
	// CTSpace Cheat
    if (g_pInput->getHoldedKey(KC) &&
		g_pInput->getHoldedKey(KT) &&
		g_pInput->getHoldedKey(KSPACE))
	{
		g_pInput->flushAll();
		for(i=0;i<m_NumPlayers;i++)
		{
			m_Player[i].pfiring = false;
			if (m_Player[i].m_playingmode)
			{
				m_Player[i].give_keycard(DOOR_YELLOW);
				m_Player[i].give_keycard(DOOR_RED);
				m_Player[i].give_keycard(DOOR_GREEN);
				m_Player[i].give_keycard(DOOR_BLUE);
				
				m_Player[i].inventory.charges = 999;
				m_Player[i].inventory.HasPogo = 1;
				m_Player[i].inventory.lives = 10;

				std::string Text;
				Text = 	"You are now cheating!\n";
				Text +=	"You got more lives\n";
				Text +=	"all the key cards, and\n";
				Text +=	"lots of ray gun charges!\n";
				 
				m_MessageBoxes.push_back(new CMessageBox(Text));
				m_paused = true;
			}
		}
		g_pVideoDriver->AddConsoleMsg("All items cheat");
	}
	
    // GOD cheat -- toggle god mode
    if ( g_pInput->getHoldedKey(KG) && g_pInput->getHoldedKey(KO) && g_pInput->getHoldedKey(KD) )
    {
    	for(i=0;i<MAX_PLAYERS;i++)
    		m_Player[i].godmode ^= 1;

    	g_pVideoDriver->DeleteConsoleMsgs();
    	if (m_Player[0].godmode)
    		g_pVideoDriver->AddConsoleMsg("God mode ON");
    	else
    		g_pVideoDriver->AddConsoleMsg("God mode OFF");

    	g_pSound->playSound(SOUND_GUN_CLICK, PLAY_FORCE);

    	// Show a message like in the original game
		m_MessageBoxes.push_back(new CMessageBox(m_Player[0].godmode ? "Godmode enabled" : "Godmode disabled"));
    	m_paused = true;
    	g_pInput->flushKeys();
    }

    if (mp_option[OPT_CHEATS].value)
    {
    	if (g_pInput->getHoldedKey(KTAB)) // noclip/revive
    	{
    		// resurrect any dead players. the rest of the KTAB magic is
    		// scattered throughout the various functions.
    		for(i=0;i<m_NumPlayers;i++)
    		{
    			if (m_Player[i].pdie)
    			{
    				m_Player[i].pdie = PDIE_NODIE;
    				m_Player[i].y -= (8<<CSF);
    			}
    			m_Player[i].pfrozentime = 0;
    		}
    	}

    	// F9 - exit level immediately
    	if(g_pInput->getPressedKey(KF9))
    	{
    		m_Player[0].level_done = LEVEL_COMPLETE;
    	}
    }

	if(g_pInput->getPressedKey(KP))
	{
		g_pSound->playSound(SOUND_GUN_CLICK, PLAY_FORCE);
		m_MessageBoxes.push_back(new CMessageBox("Game Paused"));
	}

	if(g_pInput->getPressedKey(KF1))
	{
		// Show the typical F1 Help
		// Open the menu
		mp_Menu = new CMenu( ACTIVE, m_Gamepath, m_Episode, *mp_Map, m_SavedGame, mp_option );
		mp_Menu->init(F1);
		//showF1HelpText(pCKP->Control.levelcontrol.episode, pCKP->Resources.GameDataDirectory);
	}

    // F3 - save game
    if (g_pInput->getPressedKey(KF3))
    {
		mp_Menu = new CMenu( ACTIVE, m_Gamepath, m_Episode, *mp_Map, m_SavedGame, mp_option );
		mp_Menu->init(SAVE);
    }
}

// The Ending and mortimer cutscenes for example
void CPlayGame::verifyCutscenes()
{
	// first we need to know which Episode we were on
	if(m_Episode == 1)
	{
		bool hasBattery, hasWiskey, hasJoystick, hasVaccum;
		hasBattery = hasWiskey = hasJoystick = hasVaccum = false;

		// Check if one of the Players has the items
		for( int i=0 ;i < m_NumPlayers ; i++)
		{
			hasBattery |= m_Player[i].inventory.HasBattery;
			hasWiskey |= m_Player[i].inventory.HasWiskey;
			hasJoystick |= m_Player[i].inventory.HasJoystick;
			hasVaccum |= m_Player[i].inventory.HasVacuum;
		}

		// If they have have the items, we can go home
		if(hasBattery && hasWiskey && hasJoystick && hasVaccum)
			createFinale();
	}
	else if(m_Episode == 2)
	{
		bool allCitiesSaved;
		allCitiesSaved = mp_level_completed[4];
		allCitiesSaved &= mp_level_completed[6];
		allCitiesSaved &= mp_level_completed[7];
		allCitiesSaved &= mp_level_completed[9];
		allCitiesSaved &= mp_level_completed[11];
		allCitiesSaved &= mp_level_completed[13];
		allCitiesSaved &= mp_level_completed[15];
		allCitiesSaved &= mp_level_completed[16];

		if(allCitiesSaved)
			createFinale();
	}
	else if(m_Episode == 3)
	{
		if(mp_level_completed[16] == true) // If this level is completed, Mortimer has been killed!
			createFinale();
	}
}

void CPlayGame::createFinale()
{
	if(m_Episode == 1)
	{
		mp_Finale = new CEndingEp1(mp_Map, m_Player);
	}
	else if(m_Episode == 2)
	{
		mp_Finale = new CEndingEp2(mp_Map, m_Player);
	}
	else if(m_Episode == 3)
	{
		mp_Finale = new CEndingEp3(mp_Map, m_Player);
	}
}

void CPlayGame::collectHighScoreInfo()
{
	if(m_Episode == 1)
	{
		bool extra[4];

		extra[0] = m_Player[0].inventory.HasJoystick;
		extra[1] = m_Player[0].inventory.HasBattery;
		extra[2] = m_Player[0].inventory.HasVacuum;
		extra[3] = m_Player[0].inventory.HasWiskey;

		mp_HighScores->writeEP1HighScore(m_Player[0].inventory.score, extra);
	}
	else if(m_Episode == 2)
	{
		// episode 2: game is won when all cities are saved
		int saved_cities=0;
		if (mp_level_completed[4]) saved_cities++;
		if (mp_level_completed[6]) saved_cities++;
		if (mp_level_completed[7]) saved_cities++;
		if (mp_level_completed[13]) saved_cities++;
		if (mp_level_completed[11]) saved_cities++;
		if (mp_level_completed[9]) saved_cities++;
		if (mp_level_completed[15]) saved_cities++;
		if (mp_level_completed[16]) saved_cities++;

		mp_HighScores->writeEP2HighScore(m_Player[0].inventory.score, saved_cities);
	}
	else
		mp_HighScores->writeHighScoreCommon(m_Player[0].inventory.score);
}


// This function draws the objects that need to be seen on the screen
void CPlayGame::drawObjects()
{
	int i;
	int x,y,o,tl,xsize,ysize;
	int xa,ya;
	
	if(m_hideobjects) return;

	// copy player data to their associated objects show they can get drawn
	// in the object-drawing loop with the rest of the objects
	for( i=0 ;i < m_NumPlayers ; i++)
	{
		o = m_Player[i].m_player_number;
		
		if (!m_Player[i].hideplayer && !m_Player[i].beingteleported)
			m_Object.at(o).sprite = m_Player[i].playframe;
		else
			m_Object.at(o).sprite = m_NumSprites-1;
		
		m_Object.at(o).x = m_Player[i].x;
		m_Object.at(o).y = m_Player[i].y;
	}
	
	// draw all objects. drawn in reverse order because the player sprites
	// are in the first few indexes and we want them to come out on top.
	CObject *p_object;
	for ( i=m_Object.size()-1 ; i>=0 ; i--)
	{
		p_object = &m_Object[i];
		
		if (p_object->exists && p_object->onscreen)
		{
			CSprite &Sprite = *g_pGfxEngine->Sprite[p_object->sprite];
			p_object->scrx = (p_object->x>>STC)-mp_Map->m_scrollx;
			p_object->scry = (p_object->y>>STC)-mp_Map->m_scrolly;

			Sprite.drawSprite( g_pVideoDriver->getBlitSurface(), p_object->scrx, p_object->scry );

			p_object->bboxX1 = Sprite.m_bboxX1;
			p_object->bboxX2 = Sprite.m_bboxX2;
			p_object->bboxY1 = Sprite.m_bboxY1;
			p_object->bboxY2 = Sprite.m_bboxY2;

	        if (p_object->honorPriority)
	        {
	        	CSprite *sprite = g_pGfxEngine->Sprite[p_object->sprite];
	            // handle priority tiles and tiles with masks
	            // get the upper-left coordinates to start checking for tiles
	            x = (p_object->x>>CSF);
	            y = (p_object->y>>CSF);
				
	            // get the xsize/ysize of this sprite--round up to the nearest 16
	            xsize = ((sprite->getWidth()>>4)<<4);
	            if (xsize != sprite->getWidth()) xsize+=16;
				
	            ysize = ((g_pGfxEngine->Sprite[p_object->sprite]->getHeight()>>4)<<4);
	            if (ysize != sprite->getHeight()) ysize+=16;
				
	            tl = mp_Map->at(x,y);
	            x<<=4;
	            y<<=4;
				
	            // now redraw any priority/masked tiles that we covered up
	            // with the sprite
	            SDL_Surface *sfc = g_pVideoDriver->getBlitSurface();
	            SDL_Rect sfc_rect;
	            sfc_rect.w = sfc_rect.h = 16;
				
	            for(ya=0;ya<=ysize;ya+=16)
	            {
					for(xa=0;xa<=xsize;xa+=16)
					{
						tl = mp_Map->at((x+xa)>>4,(y+ya)>>4);
						if(mp_Map->mp_tiles[tl].behaviour == -2)
							g_pGfxEngine->Tilemap->drawTile(sfc, x+xa-mp_Map->m_scrollx, y+ya-mp_Map->m_scrolly, tl+1);
						else if (mp_Map->mp_tiles[tl].behaviour == -1)
							g_pGfxEngine->Tilemap->drawTile(sfc, x+xa-mp_Map->m_scrollx, y+ya-mp_Map->m_scrolly, tl);
					}
	            }
	        }
		}
	}
}
////
// Getters
////

bool CPlayGame::getEndGame() { return m_endgame; }
bool CPlayGame::getStartGame() { return m_startgame; }
bool CPlayGame::getExitEvent() { return m_exitgame; }
char CPlayGame::getEpisode() { return m_Episode; }
char CPlayGame::getNumPlayers() { return m_NumPlayers; }
char CPlayGame::getDifficulty() { return m_Difficulty; }

////
// Cleanup Routine
////
void CPlayGame::cleanup()
{
	SAFE_DELETE(mp_Map);
	SAFE_DELETE(mp_ObjectAI);
}

CPlayGame::~CPlayGame() {
	m_Player.clear();
	if(mp_Finale) delete mp_Finale;
	mp_Finale = NULL;
	if(mp_gameoverbmp) delete mp_gameoverbmp;
	mp_gameoverbmp = NULL;
}
