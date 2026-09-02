#pragma once
#include <vector>
#include "../../Game/Mapper.h"

class Settings {
public:
	bool m_Destruct{ false };

	struct
	{
		struct
		{
			bool m_Enabled{ false };

			int m_Bind{ NULL };

			bool m_ColorBar{ false };
			bool m_Rainbow{ false };
			bool m_Background{ false };

			float m_RainbowSaturation{ 0.5f };
			float m_Color[3]{ 89.f / 255.f, 161.f / 255.f, 236.f / 255.f };
		}ArrayList;
	}Visuals;

	struct
	{
		struct
		{
			bool m_Enabled{ false };
			int m_Bind{ NULL };

			int m_Average{ 14 };

			bool m_OnlyWeapon{ true };
			bool m_InInventory{ false };
			bool m_SmartBlockHit{ false };
		}Clicker;
	}Combat;
};

extern GameVersions g_GameVersion;