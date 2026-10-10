#include "regram/nagram/export/range_model.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

struct Dated {
	TimeId date = 0;
};

} // namespace

void TestExport() {
	using namespace Nagram::Export;
	const auto at = [](int id, int newer) { return Anchor{ id, newer }; };

	Require(RangeCount(500, std::nullopt, std::nullopt) == 500,
		"export counts the whole chat without dates");
	Require(RangeCount(500, at(401, 99), std::nullopt) == 100,
		"export counts from the start date");
	Require(RangeCount(500, std::nullopt, at(451, 49)) == 450,
		"export counts up to the end date");
	Require(RangeCount(500, at(401, 99), at(451, 49)) == 50,
		"export counts inside both dates");
	Require(RangeCount(500, at(401, 99), Anchor()) == 100,
		"export counts to the end when nothing is past the end date");
	Require(RangeCount(500, Anchor(), std::nullopt) == 0
		&& RangeCount(500, at(401, 99), at(401, 99)) == 0
		&& RangeCount(0, std::nullopt, std::nullopt) == 0,
		"export empty ranges");
	Require(RangeCount(500, Anchor{ 401 }, at(451, 49)) == 450
		&& RangeCount(500, at(401, 99), Anchor{ 451 }) == 100,
		"export count stays an upper bound without offsets");
	Require(RangeCount(500, at(401, 99), at(402, 99)) == 1
		&& RangeCount(500, at(401, 900), std::nullopt) == 500,
		"export count survives inexact offsets");

	auto asked = std::vector<TimeId>();
	auto range = Range();
	const auto lookup = [&](TimeId date, Fn<void(Anchor)> done) {
		asked.push_back(date);
		done((date == 1000) ? at(401, 99) : at(451, 49));
	};
	const auto done = [&](Range result) { range = result; };
	ResolveRange(500, 1000, 2000, lookup, done);
	Require(asked == std::vector<TimeId>{ 1000, 2000 }
		&& range.firstId == 401
		&& range.count == 50, "export range with both dates");
	asked.clear();
	ResolveRange(500, 0, 2000, lookup, done);
	Require(asked == std::vector<TimeId>{ 2000 }
		&& range.firstId == 1
		&& range.count == 450, "export range with an end date only");
	asked.clear();
	ResolveRange(500, 0, 0, lookup, done);
	Require(asked.empty() && range.firstId == 1 && range.count == 500,
		"export range without dates asks nothing");
	ResolveRange(0, 1000, 2000, lookup, done);
	Require(asked.empty() && range.firstId == 1 && range.count == 0,
		"export range of a skipped chat asks nothing");

	using List = std::vector<Dated>;
	Require(PastRange(List{ { 2000 }, { 2001 } }, 2000),
		"export stops once a slice is past the end date");
	Require(!PastRange(List{ { 1999 }, { 2001 } }, 2000)
		&& !PastRange(List{ { 0 }, { 2001 } }, 2000)
		&& !PastRange(List(), 2000)
		&& !PastRange(List{ { 2001 } }, 0),
		"export keeps going while a slice may hold wanted messages");

	std::cout << "PASS: Nagram export range" << std::endl;
}
