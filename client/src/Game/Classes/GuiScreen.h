#pragma once

class GuiScreen
{
public:
    bool IsInventory(JNIEnv* env);
    bool IsChat(JNIEnv* env);
    bool IsContainerGui(JNIEnv* env);

    bool IsInMenu() const {
        return this != NULL;
    }
};