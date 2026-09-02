#pragma once

enum GameVersions {
	LUNAR_1_7_10 = 6,
	LUNAR_1_8_9 = 8
};

class Mapper
{
public:
	static void			Initialize(const GameVersions version);
	static std::string	Get(const char* mapping, int type = 1);
};