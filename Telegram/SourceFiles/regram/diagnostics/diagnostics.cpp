#include "regram/diagnostics/diagnostics.h"

#include "base/timer.h"
#include "core/application.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "regram/filters/runtime.h"
#include "regram/nagram/network/options.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"

#include <QtCore/QElapsedTimer>
#include <array>
#include <atomic>
#include <limits>
#include <map>

#include "styles/style_layers.h"
#include "styles/style_nagram_compose.h"
#include "styles/style_settings.h"

namespace Regram::Diagnostics {
namespace {

constexpr auto kBucketTime = crl::time(100);
constexpr auto kWindow = crl::time(3000);
constexpr auto kRefresh = crl::time(1000);
constexpr auto kMessageCount = 100;

struct Bucket {
	crl::time tick = -1;
	uint64 bytes = 0;
};

struct Meter {
	uint64 received = 0;
	std::array<Bucket, kWindow / kBucketTime + 1> buckets;
};

struct Benchmark {
	std::atomic<uint64> generation = 0;
};

struct BoxState {
	base::Timer timer;
	std::shared_ptr<Benchmark> benchmark = std::make_shared<Benchmark>();
	rpl::variable<QString> statistics;
	rpl::variable<QString> result;
};

std::map<Main::Session*, Meter> &Meters() {
	static auto result = std::map<Main::Session*, Meter>();
	return result;
}

uint64 AddBytes(uint64 before, uint64 bytes) {
	return before + std::min(bytes, std::numeric_limits<uint64>::max() - before);
}

QString TransferText() {
	auto lines = QStringList();
	for (const auto account : Core::App().domain().orderedAccounts()) {
		if (const auto session = account->maybeSession()) {
			const auto value = Transfers(session);
			lines.push_back(tr::lng_regram_diagnostics_transfer(tr::now,
				lt_name, session->user()->name(),
				lt_amount, QString::number(value.receivedBytes / 1048576., 'f', 2),
				lt_value, QString::number(value.bytesPerSecond / 1048576., 'f', 2)));
		}
	}
	return lines.join(u'\n');
}

void RunBenchmark(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		not_null<BoxState*> state) {
	const auto stored = Filters::Read(session);
	if (!stored.error.isEmpty()) {
		box->showToast(stored.error);
		return;
	}
	const auto token = ++state->benchmark->generation;
	const auto benchmark = state->benchmark;
	state->result = tr::lng_regram_diagnostics_testing(tr::now);
	const auto weak = base::weak_qptr<Ui::GenericBox>(box);
	crl::async([=, rules = stored.rules]() mutable {
		for (auto &rule : rules) {
			rule.scopes.clear();
			rule.unresolvedScopes.clear();
		}
		auto engine = Filters::Engine();
		if (benchmark->generation != token || !engine.setRules(std::move(rules))) {
			return;
		}
		const auto base = u"这是一段性能测试消息 Hello Regram 😀 1234567。"_q.repeated(15);
		auto samples = std::vector<std::u16string>();
		for (auto index = 0; index != kMessageCount; ++index) {
			samples.push_back((base + u" #"_q + QString::number(index)).toStdU16String());
		}
		auto limited = 0;
		auto clock = QElapsedTimer();
		clock.start();
		for (const auto &sample : samples) {
			if (benchmark->generation != token) {
				return;
			}
			limited += engine.evaluate(sample, std::nullopt).limited ? 1 : 0;
		}
		const auto average = clock.nsecsElapsed() / 1000000. / kMessageCount;
		crl::on_main(weak, [=] {
			if (benchmark->generation == token) {
				state->result = tr::lng_regram_diagnostics_result(tr::now,
					lt_value, QString::number(average, 'f', 2),
					lt_amount, QString::number(limited));
			}
		});
	});
}

} // namespace

void RecordMediaReceived(not_null<Main::Session*> session, int64 bytes) {
	if (bytes <= 0) {
		return;
	}
	auto &meters = Meters();
	auto found = meters.find(session);
	const auto created = found == meters.end();
	if (found == meters.end()) {
		found = meters.emplace(session, Meter()).first;
	}
	auto &meter = found->second;
	meter.received = AddBytes(meter.received, bytes);
	const auto tick = crl::now() / kBucketTime;
	auto &bucket = meter.buckets[tick % meter.buckets.size()];
	if (bucket.tick != tick) {
		bucket = { .tick = tick };
	}
	bucket.bytes = AddBytes(bucket.bytes, bytes);
	if (created) {
		session->lifetime().add([session] { Meters().erase(session); });
	}
}

TransferSnapshot Transfers(not_null<Main::Session*> session) {
	const auto found = Meters().find(session);
	if (found == Meters().end()) {
		return {};
	}
	const auto &meter = found->second;
	const auto now = crl::now() / kBucketTime;
	auto recent = uint64(0);
	for (const auto &bucket : meter.buckets) {
		if (bucket.tick >= 0 && bucket.tick <= now
			&& now - bucket.tick < kWindow / kBucketTime) {
			recent = AddBytes(recent, bucket.bytes);
		}
	}
	return { .receivedBytes = meter.received, .bytesPerSecond = recent / 3 };
}

void SettingsBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	box->setTitle(tr::lng_regram_diagnostics());
	const auto state = box->lifetime().make_state<BoxState>();
	box->lifetime().add([benchmark = state->benchmark] { ++benchmark->generation; });
	session->lifetime().add([weak = base::weak_qptr<Ui::GenericBox>(box)] {
		if (weak) {
			weak->closeBox();
		}
	});
	state->statistics = TransferText();
	box->addRow(object_ptr<Ui::FlatLabel>(box, state->statistics.value(), st::boxLabel));
	state->timer.setCallback([=] { state->statistics = TransferText(); });
	state->timer.callEach(kRefresh);
	const auto boosted = Nagram::ForDevice().Get(Nagram::Network::kDownloadSpeedBoost) != u"none"_q;
	box->addRow(object_ptr<Ui::FlatLabel>(box,
		boosted ? tr::lng_regram_diagnostics_boost_on() : tr::lng_regram_diagnostics_boost_off(),
		st::boxLabel));
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_regram_diagnostics_about(), st::boxLabel));
	const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_regram_diagnostics_run(), st::settingsButtonNoIcon));
	button->setClickedCallback([=] { RunBenchmark(box, session, state); });
	state->result = tr::lng_regram_diagnostics_idle(tr::now);
	box->addRow(object_ptr<Ui::FlatLabel>(box, state->result.value(), st::boxLabel))
		->setFixedHeight(st::nagramDiagnosticStatusHeight);
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_regram_diagnostics_filter_about(), st::boxLabel));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Regram::Diagnostics
