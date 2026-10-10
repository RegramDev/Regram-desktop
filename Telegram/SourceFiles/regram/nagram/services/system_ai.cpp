#include "regram/nagram/services/system_ai.h"

#include "base/timer.h"
#include "boxes/compose_ai_box.h"
#include "lang/lang_keys.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"

#include "styles/style_layers.h"

#ifdef NAGRAM_SYSTEM_AI
extern "C" {
int NagramSystemAiAvailability();
void NagramSystemAiStart(
	uint64 id,
	const char *text,
	int mode,
	void *context,
	void (*done)(void*, int, const char*));
void NagramSystemAiCancel(uint64 id);
} // extern "C"
#endif

namespace Nagram {
namespace {

constexpr auto kTimeout = crl::time(120000);

class Request final {
public:
	void start(QString text, int mode, Fn<void(int, QString)> done);
	void cancel();
	~Request();

private:
	uint64 _id = 0;

};

void Request::start(QString text, int mode, Fn<void(int, QString)> done) {
	cancel();
#ifdef NAGRAM_SYSTEM_AI
	static auto next = uint64(0);
	_id = ++next;
	using Callback = Fn<void(int, QString)>;
	NagramSystemAiStart(_id, text.toUtf8().constData(), mode,
		new Callback(std::move(done)),
		[](void *context, int status, const char *text) {
			const auto callback = std::unique_ptr<Callback>(
				static_cast<Callback*>(context));
			(*callback)(status, QString::fromUtf8(text));
		});
#else
	done(1, QString());
#endif
}

void Request::cancel() {
#ifdef NAGRAM_SYSTEM_AI
	if (const auto id = base::take(_id)) {
		NagramSystemAiCancel(id);
	}
#endif
}

Request::~Request() {
	cancel();
}

} // namespace

int SystemAiAvailability() {
#ifdef NAGRAM_SYSTEM_AI
	return NagramSystemAiAvailability();
#else
	return 1;
#endif
}

QString SystemAiStatusText(int status) {
	switch (status) {
	case 0: return tr::lng_nagram_system_ai_available(tr::now);
	case 1: return tr::lng_nagram_system_ai_unsupported(tr::now);
	case 2: return tr::lng_nagram_system_ai_device(tr::now);
	case 3: return tr::lng_nagram_system_ai_disabled(tr::now);
	case 4: return tr::lng_nagram_system_ai_loading(tr::now);
	case 6: return tr::lng_nagram_system_ai_size(tr::now);
	case 7: return tr::lng_nagram_system_ai_cancelled(tr::now);
	default: return tr::lng_nagram_system_ai_failed(tr::now);
	}
}

void ShowSystemAi(
		std::shared_ptr<Ui::Show> show,
		HistoryView::Controls::ComposeAiBoxArgs &&args) {
	const auto available = SystemAiAvailability();
	if (available != 0) {
		auto box = Ui::MakeInformBox(SystemAiStatusText(available));

		show->showBox(std::move(box));
		return;
	}
	const auto text = args.text.text;
	if (text.trimmed().isEmpty() || text.size() > 2000
		|| text.contains(QChar(0))
		|| QString::fromUtf8(text.toUtf8()) != text) {
		show->showBox(Ui::MakeInformBox(SystemAiStatusText(6)));
		return;
	}
	show->showBox(Box([=,
			apply = std::move(args.apply),
			canApply = std::move(args.canApply)](
			not_null<Ui::GenericBox*> box) {

		struct State {
			Request request;
			base::Timer timer;
			QString result;
			int generation = 0;
			bool loading = false;
		};
		const auto state = box->lifetime().make_state<State>();
		box->setTitle(tr::lng_nagram_system_ai());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_system_ai_preview_about(), st::boxLabel));
		const auto group = std::make_shared<Ui::RadiobuttonGroup>(0);
		const auto modes = std::array{
			tr::lng_nagram_system_ai_proofread(tr::now),
			tr::lng_nagram_system_ai_rewrite(tr::now),
			tr::lng_nagram_system_ai_summary(tr::now),
		};
		for (auto i = 0; i != int(modes.size()); ++i) {
			box->addRow(object_ptr<Ui::Radiobutton>(box, group, i, modes[i]));
		}
		const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
			box, st::boxLabel));
		result->setSelectable(true);
		box->addButton(tr::lng_nagram_system_ai_generate(), [=] {
			if (state->loading) {
				return;
			}
			state->result.clear();
			state->loading = true;
			const auto generation = ++state->generation;
			result->setText(tr::lng_contacts_loading(tr::now));
			state->timer.setCallback([=] {
				++state->generation;
				state->request.cancel();
				state->loading = false;
				result->setText(tr::lng_nagram_system_ai_timeout(tr::now));
			});
			state->timer.callOnce(kTimeout);
			state->request.start(text, group->current(),
				crl::guard(box, [=](int status, QString value) {
					if (generation != state->generation) {
						return;
					}
					state->timer.cancel();
					state->loading = false;
					if (!status && (value.trimmed().isEmpty()
						|| value.size() > 16384 || value.contains(QChar(0))
						|| QString::fromUtf8(value.toUtf8()) != value)) {
						status = 8;
					}
					if (status) {
						result->setText(SystemAiStatusText(status));
					} else {
						state->result = std::move(value);
						result->setText(state->result);
					}
				}));
		});
		box->addButton(tr::lng_nagram_translate_apply(), [=] {
			if (state->loading || state->result.isEmpty()) {
				return;
			}
			if (!canApply || !canApply()) {
				box->showToast(tr::lng_nagram_draft_changed(tr::now));
				return;
			}
			apply(tr::marked(state->result));
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram
