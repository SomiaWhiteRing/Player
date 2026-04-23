#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "game_interpreter.h"
#include "graphics.h"
#include "main_data.h"
#include "mock_game.h"
#include "output.h"

namespace {

using CapturedLog = std::pair<LogLevel, std::string>;

void CaptureLog(LogLevel lvl, std::string const& message, LogCallbackUserData userdata) {
	auto* logs = static_cast<std::vector<CapturedLog>*>(userdata);
	logs->emplace_back(lvl, message);
}

} // namespace

TEST_SUITE_BEGIN("Game_Interpreter");

TEST_CASE("ChangePartyMember ignores actor zero without warning") {
	lcf::Data::actors.clear();
	lcf::Data::actors.resize(1);
	lcf::Data::actors[0].initial_level = 1;

	Graphics::Init();
	MockGame game(MockMap::eNone);

	Main_Data::game_party->AddActor(1);
	REQUIRE(Main_Data::game_party->GetBattlerCount() == 1);

	std::vector<CapturedLog> logs;
	const auto old_level = Output::GetLogLevel();
	Output::SetLogLevel(LogLevel::Warning);
	Output::SetLogCallback(CaptureLog, &logs);

	lcf::rpg::EventCommand cmd;
	cmd.code = static_cast<int32_t>(lcf::rpg::EventCommand::Code::ChangePartyMembers);
	cmd.parameters = {1, 0, 0};

	Game_Interpreter interpreter;
	CHECK(interpreter.ExecuteCommand(cmd));
	CHECK(Main_Data::game_party->GetBattlerCount() == 1);

	Output::SetLogCallback(nullptr);
	Output::SetLogLevel(old_level);
	Graphics::Quit();

	CHECK(logs.empty());
}

TEST_SUITE_END();
