#include "regram/nagram/compose/confirm.h"

#include "data/data_document.h"
#include "lang/lang_keys.h"
#include "regram/nagram/compose/options.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/show.h"

#include <crl/crl_time.h>

namespace Nagram::Compose {
namespace {

constexpr auto kConfirmedTimeout = crl::time(60'000);

struct Confirmed {
	DocumentId id = 0;
	crl::time at = 0;
};

Confirmed LastConfirmed;

// WHY: payment approval re-enters the send, so one confirmation must
// cover the retry of the same document.
[[nodiscard]] bool TakeConfirmed(not_null<DocumentData*> document) {
	const auto matches = (LastConfirmed.id == document->id)
		&& (crl::now() - LastConfirmed.at < kConfirmedTimeout);
	LastConfirmed = {};
	return matches;
}

} // namespace

bool ConfirmBeforeSend(
		std::shared_ptr<Ui::Show> show,
		DocumentData *document,
		Fn<void()> resend) {
	if (!document) {
		return false;
	}
	const auto sticker = document->sticker()
		&& ForDevice().Get(kConfirmSticker);
	const auto gif = !document->sticker()
		&& (document->isAnimation() || document->isGifv())
		&& ForDevice().Get(kConfirmGif);
	if ((!sticker && !gif) || TakeConfirmed(document)) {
		return false;
	}
	const auto id = document->id;
	show->showBox(Ui::MakeConfirmBox({
		.text = sticker
			? tr::lng_nagram_confirm_send_sticker()
			: tr::lng_nagram_confirm_send_gif(),
		.confirmed = [=](Fn<void()> close) {
			close();
			LastConfirmed = { .id = id, .at = crl::now() };
			resend();
		},
		.confirmText = tr::lng_send_button(),
	}));
	return true;
}

} // namespace Nagram::Compose
