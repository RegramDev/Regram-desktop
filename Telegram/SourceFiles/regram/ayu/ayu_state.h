// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "history/history.h"
#include "history/history_item.h"

namespace Main {
class Session;
} // namespace Main

namespace AyuState {

void setDisableGhostModeOnStoryClose(Main::Session *session);
void disableGhostModeOnStoryClose(Main::Session *session);

}
