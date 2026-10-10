#pragma once

#include "base/basic_types.h"

#include <gsl/pointers>

#include <memory>

class DocumentData;
class PeerData;

namespace Main {
class SessionShow;
} // namespace Main

namespace Ui {
class InputField;
class Show;
} // namespace Ui

namespace Nagram::Compose {

// Returns true when a confirmation box took over the send.
[[nodiscard]] bool ConfirmBeforeSend(
	std::shared_ptr<Ui::Show> show,
	DocumentData *document,
	Fn<void()> resend);
[[nodiscard]] bool TranslateBeforeSend(
	std::shared_ptr<Main::SessionShow> show,
	gsl::not_null<PeerData*> peer,
	Ui::InputField *field,
	Fn<void()> resend);

} // namespace Nagram::Compose
