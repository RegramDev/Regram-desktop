#pragma once

#include "base/basic_types.h"
#include "spellcheck/spellcheck_types.h"
#include "ui/text/text_entity.h"

#include <memory>
#include <optional>
#include <gsl/pointers>

namespace Main { class SessionShow; }
namespace Ui {
class GenericBox;
class InputField;
} // namespace Ui

namespace Nagram {

using TranslationButtons = Fn<void(
	not_null<Ui::GenericBox*> box,
	TextWithTags original,
	Fn<std::optional<TextWithEntities>()> result)>;

void ShowTranslationBox(
	std::shared_ptr<Main::SessionShow> show,
	not_null<Ui::InputField*> field,
	LanguageId to,
	TranslationButtons buttons);
void SetDraftTranslation(
	not_null<Ui::InputField*> field,
	const TextWithEntities &translation);
void InstallDraftTranslation(
	not_null<Ui::InputField*> field,
	std::shared_ptr<Main::SessionShow> show);

} // namespace Nagram
