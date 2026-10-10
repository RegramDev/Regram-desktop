#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <gsl/pointers>
#include <rpl/rpl.h>

#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Core {
class Settings;
} // namespace Core
namespace Main {
class Session;
} // namespace Main
namespace Storage {
class Account;
} // namespace Storage

namespace Nagram {

class Options;
[[nodiscard]] Options &ForDevice();

enum class Scope { Device, Account };
enum class Category { Interface, Chats, Messages, Compose, Menu, Media, Privacy, Services, Rules, Network };
enum class Flag : unsigned {
	None = 0,
	RequiresRestart = 1,
	Exportable = 2,
	Hidden = 4,
	RefreshMessageView = 8,
	RefreshDialogList = 16,
	RefreshComposeButtons = 32,
	LocalOnly = 64,
};

template <typename Type>
struct Option {
	std::string_view key;
	Scope scope;
	Type fallback;
	Category category;
	std::string_view titleKey;
	unsigned flags = 0;
	bool (*validate)(const Type &) = nullptr;
	Type (*imported)(const Type &) = nullptr;
};

struct OptionInfo {
	enum class ValueType { Boolean, Integer, String, Object };
	std::string_view key;
	Scope scope;
	Category category;
	std::string_view titleKey;
	unsigned flags;
	ValueType type;
	QByteArray fallbackRaw;
	std::function<bool(const QJsonValue &)> accepts;
	std::function<QJsonValue(const QJsonValue &)> imported;
};

class Registry final {
public:
	template <typename Type>
	[[nodiscard]] bool Add(const Option<Type> &option) {
		if (!option.key.starts_with("nagram.") || option.titleKey.empty()) {
			return false;
		}
		for (const auto &entry : _entries) {
			if (entry.key == option.key) {
				return false;
			}
		}
		static_assert(std::is_same_v<Type, bool>
			|| std::is_same_v<Type, int>
			|| std::is_same_v<Type, QString>
			|| std::is_same_v<Type, QByteArray>);
		const auto type = [] {
			if constexpr (std::is_same_v<Type, bool>) {
				return OptionInfo::ValueType::Boolean;
			} else if constexpr (std::is_same_v<Type, int>) {
				return OptionInfo::ValueType::Integer;
			} else if constexpr (std::is_same_v<Type, QString>) {
				return OptionInfo::ValueType::String;
			} else {
				return OptionInfo::ValueType::Object;
			}
		}();
		const auto fallbackRaw = [&] {
			if constexpr (std::is_same_v<Type, bool>) {
				return QByteArray(option.fallback ? "1" : "0");
			} else if constexpr (std::is_same_v<Type, int>) {
				return QByteArray::number(option.fallback);
			} else if constexpr (std::is_same_v<Type, QString>) {
				return "s" + option.fallback.toUtf8();
			} else {
				return option.fallback;
			}
		}();
		const auto accepts = [validate = option.validate](const QJsonValue &value) {
			if constexpr (std::is_same_v<Type, bool>) {
				return value.isBool() && (!validate || validate(value.toBool()));
			} else if constexpr (std::is_same_v<Type, int>) {
				const auto parsed = value.toInt(std::numeric_limits<int>::min());
				return value.isDouble()
					&& value.toDouble() == double(parsed)
					&& (!validate || validate(parsed));
			} else if constexpr (std::is_same_v<Type, QString>) {
				const auto parsed = value.toString();
				return value.isString() && !parsed.contains(u'\n')
					&& !parsed.contains(u'\r')
					&& (!validate || validate(parsed));
			} else {
				return value.isObject() && (!validate || validate(
					QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)));
			}
		};
		auto imported = std::function<QJsonValue(const QJsonValue &)>();
		if constexpr (std::is_same_v<Type, QByteArray>) {
			if (const auto adjust = option.imported) {
				imported = [adjust](const QJsonValue &value) {
					return QJsonValue(QJsonDocument::fromJson(adjust(
						QJsonDocument(value.toObject()).toJson(
							QJsonDocument::Compact))).object());
				};
			}
		}
		const auto exportable = (option.scope == Scope::Device)
			&& !(option.flags & static_cast<unsigned>(Flag::Hidden));
		_entries.push_back({
			option.key, option.scope, option.category,
			option.titleKey,
			option.flags | (exportable ? static_cast<unsigned>(Flag::Exportable) : 0),
			type, fallbackRaw, accepts, std::move(imported) });
		return true;
	}

	[[nodiscard]] std::span<const OptionInfo> All() const {
		return _entries;
	}
	[[nodiscard]] const OptionInfo *Find(std::string_view key) const {
		for (const auto &entry : _entries) {
			if (entry.key == key) {
				return &entry;
			}
		}
		return nullptr;
	}
	[[nodiscard]] bool HasFlag(std::string_view key, Flag flag) const {
		for (const auto &entry : _entries) {
			if (entry.key == key) {
				return (entry.flags & static_cast<unsigned>(flag)) != 0;
			}
		}
		return false;
	}

private:
	std::vector<OptionInfo> _entries;
};

class RawPrefs {
public:
	virtual ~RawPrefs() = default;
	[[nodiscard]] virtual QByteArray read(std::string_view key) = 0;
	virtual void write(std::string_view key, const QByteArray &value) = 0;
	virtual void clear(std::string_view key) = 0;
};

class DevicePrefs final : public RawPrefs {
public:
	explicit DevicePrefs(Core::Settings &settings);
	[[nodiscard]] QByteArray read(std::string_view key) override;
	void write(std::string_view key, const QByteArray &value) override;
	void clear(std::string_view key) override;

private:
	Core::Settings &_settings;
};

class AccountPrefs final : public RawPrefs {
public:
	explicit AccountPrefs(Storage::Account &account);
	[[nodiscard]] QByteArray read(std::string_view key) override;
	void write(std::string_view key, const QByteArray &value) override;
	void clear(std::string_view key) override;

private:
	Storage::Account &_account;
};

class Options final {
	friend class Exchange;
public:
	explicit Options(RawPrefs &prefs, Scope scope = Scope::Device)
	: _prefs(prefs), _scope(scope) { }

	template <typename Type>
	[[nodiscard]] Type Get(const Option<Type> &option) {
		Expects(option.scope == _scope);
		static_assert(std::is_same_v<Type, bool>
			|| std::is_same_v<Type, int>
			|| std::is_same_v<Type, QString>
			|| std::is_same_v<Type, QByteArray>);
		const auto raw = _prefs.read(option.key);
		if (raw.isEmpty()) {
			_invalidKeys.erase(option.key);
			return option.fallback;
		}
		auto value = std::optional<Type>();
		if constexpr (std::is_same_v<Type, bool>) {
			if (raw == "1" || raw == "0") {
				value = (raw == "1");
			}
		} else if constexpr (std::is_same_v<Type, int>) {
			auto ok = false;
			const auto parsed = raw.toInt(&ok);
			if (ok && QByteArray::number(parsed) == raw) {
				value = parsed;
			}
		} else if constexpr (std::is_same_v<Type, QString>) {
			if (raw.startsWith('s')) {
				const auto bytes = raw.mid(1);
				const auto parsed = QString::fromUtf8(bytes);
				if (parsed.toUtf8() == bytes && !parsed.contains(u'\n')
					&& !parsed.contains(u'\r')) {
					value = parsed;
				}
			}
		} else {
			value = raw;
		}
		if (!value || !Valid(option, *value)) {
			if (_invalidKeys.insert(option.key).second) {
				_readErrors.fire_copy(option.key);
			}
			return option.fallback;
		}
		_invalidKeys.erase(option.key);
		return *value;
	}

	template <typename Type>
	[[nodiscard]] bool Set(const Option<Type> &option, const Type &value) {
		if (option.scope != _scope
			|| (option.validate && !option.validate(value))) {
			return false;
		}
		if constexpr (std::is_same_v<Type, QString>) {
			if (value.contains(u'\n') || value.contains(u'\r')) {
				return false;
			}
		}
		const auto old = _prefs.read(option.key);
		if (value == option.fallback) {
			_prefs.clear(option.key);
		} else if constexpr (std::is_same_v<Type, bool>) {
			_prefs.write(option.key, value ? "1" : "0");
		} else if constexpr (std::is_same_v<Type, int>) {
			_prefs.write(option.key, QByteArray::number(value));
		} else if constexpr (std::is_same_v<Type, QString>) {
			_prefs.write(option.key, "s" + value.toUtf8());
		} else {
			_prefs.write(option.key, value);
		}
		_invalidKeys.erase(option.key);
		if (old != _prefs.read(option.key)) {
			_changes.fire_copy(option.key);
		}
		return true;
	}

	template <typename Type>
	[[nodiscard]] rpl::producer<Type> Value(const Option<Type> &option) {
		return rpl::single(Get(option)) | rpl::then(
			_changes.events() | rpl::filter([key = option.key](auto changed) {
				return changed == key;
			}) | rpl::map([this, option] { return Get(option); })
		) | rpl::distinct_until_changed();
	}

	[[nodiscard]] rpl::producer<std::string_view> readErrors() const {
		return _readErrors.events();
	}
	[[nodiscard]] rpl::producer<std::string_view> changes() const {
		return _changes.events();
	}
	[[nodiscard]] const std::set<std::string_view> &invalidKeys() const {
		return _invalidKeys;
	}

private:
	template <typename Type>
	struct Validated {
		Type value;
		bool (*validate)(const Type &) = nullptr;
		bool valid = false;
	};

	template <typename Type>
	[[nodiscard]] bool ValidCached(
			const Option<Type> &option,
			const Type &value,
			std::map<std::string_view, Validated<Type>> &cache) {
		const auto found = cache.find(option.key);
		if (found != cache.end()
			&& found->second.value == value
			&& found->second.validate == option.validate) {
			return found->second.valid;
		}
		const auto valid = option.validate(value);
		cache[option.key] = { value, option.validate, valid };
		return valid;
	}

	template <typename Type>
	[[nodiscard]] bool Valid(const Option<Type> &option, const Type &value) {
		if (!option.validate) {
			return true;
		}
		if constexpr (std::is_same_v<Type, QByteArray>) {
			return ValidCached(option, value, _validatedBytes);
		} else if constexpr (std::is_same_v<Type, QString>) {
			return ValidCached(option, value, _validatedStrings);
		} else {
			return option.validate(value);
		}
	}

	RawPrefs &_prefs;
	Scope _scope;
	rpl::event_stream<std::string_view> _changes;
	rpl::event_stream<std::string_view> _readErrors;
	std::set<std::string_view> _invalidKeys;
	std::map<std::string_view, Validated<QByteArray>> _validatedBytes;
	std::map<std::string_view, Validated<QString>> _validatedStrings;

};

[[nodiscard]] Options &ForAccount(gsl::not_null<Main::Session*> session);
[[nodiscard]] const Registry &RegisteredOptions();

} // namespace Nagram
