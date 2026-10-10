#include "sqlite3.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

using Key = std::array<unsigned char, 32>;

void Check(bool passed, const char *name) {
	if (!passed) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit(1);
	}
}

struct CloseDatabase {
	void operator()(sqlite3 *database) const {
		Check(sqlite3_close(database) == SQLITE_OK, "database closes cleanly");
	}
};

using Database = std::unique_ptr<sqlite3, CloseDatabase>;

Database Open(const std::filesystem::path &path, const Key &key) {
	auto raw = static_cast<sqlite3*>(nullptr);
	Check(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "open database");
	auto database = Database(raw);
	Check(sqlite3_key(raw, key.data(), int(key.size())) == SQLITE_OK, "apply key");
	return database;
}

void Execute(const Database &database, const char *sql) {
	Check(sqlite3_exec(database.get(), sql, nullptr, nullptr, nullptr) == SQLITE_OK,
		"execute SQL");
}

std::string Scalar(const Database &database, const char *sql) {
	auto result = std::string();
	const auto callback = [](void *context, int count, char **values, char **) {
		if (count && values[0]) {
			*static_cast<std::string*>(context) = values[0];
		}
		return 0;
	};
	Check(sqlite3_exec(database.get(), sql, callback, &result, nullptr) == SQLITE_OK,
		"read SQL result");
	return result;
}

std::string Bytes(const std::filesystem::path &path) {
	auto input = std::ifstream(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(input), {});
}

} // namespace

int main() {
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto directory = std::filesystem::temp_directory_path()
		/ ("regram-sqlcipher-tests-" + std::to_string(nonce));
	Check(std::filesystem::create_directory(directory), "create isolated test directory");
	std::cout << "Test data: " << directory << '\n';
	const auto path = directory / "history.db";
	auto key = Key();
	auto wrongKey = Key();
	key.fill(0x31);
	wrongKey.fill(0x72);
	{
		const auto database = Open(path, key);
		Check(!Scalar(database, "PRAGMA cipher_version").empty(), "SQLCipher enabled");
		Execute(database,
			"CREATE TABLE history (uid INTEGER, message TEXT);"
			"INSERT INTO history VALUES (11, 'private-regram-content'), (22, 'other-account');");
	}
	const auto original = Bytes(path);
	Check(!original.starts_with("SQLite format 3"), "database header encrypted");
	Check(original.find("private-regram-content") == std::string::npos,
		"message text encrypted");
	{
		const auto database = Open(path, wrongKey);
		Check(sqlite3_exec(database.get(), "SELECT * FROM history", nullptr,
			nullptr, nullptr) != SQLITE_OK, "wrong key rejected");
	}
	Check(Bytes(path) == original, "failed open preserves existing database");
	{
		const auto database = Open(path, key);
		Check(Scalar(database, "SELECT message FROM history WHERE uid=11")
			== "private-regram-content", "correct key works after restart");
		Check(Scalar(database, "PRAGMA integrity_check") == "ok", "database integrity");
		Execute(database, "DELETE FROM history WHERE uid=11;");
		Check(Scalar(database, "SELECT count(*) FROM history WHERE uid=11") == "0",
			"selected account cleared");
		Check(Scalar(database, "SELECT message FROM history WHERE uid=22") == "other-account",
			"other account preserved");
	}
	const auto corrupt = directory / "corrupt.db";
	std::filesystem::copy_file(path, corrupt);
	{
		auto file = std::fstream(corrupt, std::ios::in | std::ios::out | std::ios::binary);
		const auto first = file.get();
		file.seekp(0);
		file.put(char(first ^ 0x80));
	}
	const auto damaged = Bytes(corrupt);
	{
		const auto database = Open(corrupt, key);
		Check(sqlite3_exec(database.get(), "SELECT * FROM history", nullptr,
			nullptr, nullptr) != SQLITE_OK, "corrupt database rejected");
	}
	Check(Bytes(corrupt) == damaged, "corrupt database preserved");
	std::filesystem::remove_all(directory);
	std::cout << "SQLCipher restart, encryption and preservation tests passed\n";
}
