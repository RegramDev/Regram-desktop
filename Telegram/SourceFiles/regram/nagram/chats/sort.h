#pragma once

#include <QtCore/QtGlobal>

namespace Dialogs { class Entry; }
namespace Main { class Session; }
namespace Ui { class GenericBox; }
namespace rpl { class lifetime; }

namespace Nagram::Chats {

[[nodiscard]] bool SortingEnabled();
[[nodiscard]] uint64 SortKey(const Dialogs::Entry &entry, uint64 original);
void WatchSorting(gsl::not_null<Main::Session*> session, rpl::lifetime &lifetime);
void ChatSortBox(gsl::not_null<Ui::GenericBox*> box);

} // namespace Nagram::Chats
