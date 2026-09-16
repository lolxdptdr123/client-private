#include "pch.h"
#include "PingFix.h"
#include "../../Hooks/WSA.h"

void PingFix::Run(JNIEnv* env) {
    PingFix_Set(enabled);
    (void)env;
}
