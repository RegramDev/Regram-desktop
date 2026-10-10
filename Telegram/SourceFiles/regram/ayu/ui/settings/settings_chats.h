// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

namespace Builder {
class SectionBuilder;
} // namespace Builder

void BuildExtraMessages(Builder::SectionBuilder &builder);
void BuildExtraChats(Builder::SectionBuilder &builder);
void BuildExtraMedia(Builder::SectionBuilder &builder);
void BuildExtraCompose(Builder::SectionBuilder &builder);
void BuildExtraMessageMenu(Builder::SectionBuilder &builder);
} // namespace Settings
