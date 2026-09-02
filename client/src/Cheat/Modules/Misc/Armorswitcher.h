#pragma once
#include "../Module.h"
#include "armor.h"

class ArmorSwitcher : public Module {
public:
    void Run(JNIEnv* env) override;
    const char* GetName()   override { return "ArmorSwitcher"; }
    bool        IsEnabled() override { return Armor::enabled; }
    const char* GetSuffix() override { return ""; }
};