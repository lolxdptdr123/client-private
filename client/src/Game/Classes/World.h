#pragma once
#include <vector>

class Player;
class World
{
public:
	std::vector<Player*> GetPlayerEntities(JNIEnv* env);
	std::vector<jobject> GetLoadedEntities(JNIEnv* env);
	std::vector<jobject> GetLoadedTileEntities(JNIEnv* env);
};