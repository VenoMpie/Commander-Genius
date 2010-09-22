/*
 * CSavedGame.h
 *
 *  Created on: 13.08.2009
 *      Author: gerstrong
 */

#ifndef CSAVEDGAME_H_
#define CSAVEDGAME_H_

#include <string>
#include <vector>
#include <SDL.h>
#include <iostream>
#include <fstream>

#include "../CLogFile.h"
#include "../StringUtils.h"

#include "../fileio/TypeDefinitions.h"

const std::string EMPTY_STRING = "     EMPTY       ";

class CSavedGame {
public:
	enum SavedGameCommands{
		NONE, SAVE, LOAD
	};

	// Initialization
	CSavedGame();

	// Setters
	void setGameDirectory(const std::string& game_directory);
	void setEpisode(char Episode);

	// Getters
	std::vector<std::string> getSlotList();

	void convertAllOldFormats();
	bool convertOldFormat(size_t slot);
	bool IsOldButValidSaveGame(std::string fname);
	void readOldHeader(FILE *fp, byte *episode, byte *level, byte *lives, byte *num_players);
	Uint32 getSlotNumber(const std::string &filename);
	std::string getSlotName(const std::string &filename);
	Uint32 getDataSize(std::ifstream &StateFile);
	void readData(char *buffer, Uint32 size, std::ifstream &StateFile);

	bool Fileexists( int SaveSlot );
	bool prepareSaveGame( int SaveSlot, const std::string &Name);
	bool prepareLoadGame( int SaveSlot);

	// Encoder/Decoder Classes
	template <class T>
	void encodeVariable(T value);
	template <class S>
	void encodeData(S structure);
	template <class T>
	void decodeVariable(T &variable);
	template <class S>
	void decodeData(S &structure);

	void addData(byte *data, Uint32 size);
	void readDataBlock(byte *data);

	bool save();
	bool load();
	bool alreadyExits();
	
	char getCommand() { return m_Command; }

	// shutdown
	virtual ~CSavedGame();

private:
	std::string m_savedir;
	std::string m_statefilename;
	std::string m_statename;
	char m_Episode;
	char m_Command;
	Uint32 m_offset;

	std::vector<byte> m_datablock;
};

#include "CSavedGameCoder.h"

#endif /* CSAVEDGAME_H_ */
