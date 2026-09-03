#pragma once

enum GameVersions {
	LUNAR_1_7_10 = 6,
	LUNAR_1_8_9 = 8
};

enum GameLauncher {
	LAUNCHER_LUNAR = 0,
	LAUNCHER_CHEATBREAKER = 1
};

class Mapper
{
public:
	static void			Initialize(const GameVersions version);
	static std::string	Get(const char* mapping, int type = 1);
	static std::string	RemapSignature(const char* sig);
	static bool			IsCheatBreaker();
};