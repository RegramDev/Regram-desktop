#pragma once

#include <gsl/pointers>

namespace Dialogs {
class InnerWidget;
} // namespace Dialogs

namespace Nagram {

class ListRefresher final {
public:
	static void Attach(not_null<Dialogs::InnerWidget*> widget);
};

} // namespace Nagram
