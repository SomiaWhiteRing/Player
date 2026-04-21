#include "doctest.h"
#include "name_input_candidate.h"

TEST_SUITE_BEGIN("NameInputCandidates");

static lcf::rpg::EventCommand MakeCommand(
	lcf::rpg::EventCommand::Code code,
	int indent,
	std::vector<int32_t> parameters = {},
	const std::string& string = ""
) {
	lcf::rpg::EventCommand command;
	command.code = static_cast<uint32_t>(code);
	command.indent = indent;
	command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	command.string = lcf::DBString(string);
	return command;
}

static lcf::rpg::EventCommand MakeNameBranch(int indent, int actor_id, const std::string& answer) {
	return MakeCommand(lcf::rpg::EventCommand::Code::ConditionalBranch, indent, {5, actor_id, 1, 0, 0}, answer);
}

TEST_CASE("Collect immediate same-actor name branches") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeNameBranch(0, 4, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::Comment, 1),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, "musha"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	REQUIRE_EQ(candidates.size(), 2);
	CHECK_EQ(candidates[0].label, "alex");
	CHECK_EQ(candidates[0].value, "alex");
	CHECK_EQ(candidates[1].label, "musha");
	CHECK_EQ(candidates[1].value, "musha");
}

TEST_CASE("Collect skips nested branch bodies") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeNameBranch(0, 4, "alex"),
		MakeNameBranch(1, 4, "nested"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 1),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, "musha"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	REQUIRE_EQ(candidates.size(), 2);
	CHECK_EQ(candidates[0].value, "alex");
	CHECK_EQ(candidates[1].value, "musha");
}

TEST_CASE("Collect dedupes and filters blank answers") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeNameBranch(0, 4, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, " \t "),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, "\xE3\x80\x80"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeNameBranch(0, 4, "musha"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	REQUIRE_EQ(candidates.size(), 2);
	CHECK_EQ(candidates[0].value, "alex");
	CHECK_EQ(candidates[1].value, "musha");
}

TEST_CASE("Collect returns empty when next command is not a matching branch") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeCommand(lcf::rpg::EventCommand::Code::Comment, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	CHECK(candidates.empty());
}

TEST_CASE("Collect stops at first non-matching sibling command") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeNameBranch(0, 4, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
		MakeCommand(lcf::rpg::EventCommand::Code::Comment, 0),
		MakeNameBranch(0, 4, "musha"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	REQUIRE_EQ(candidates.size(), 1);
	CHECK_EQ(candidates[0].value, "alex");
}

TEST_CASE("Collect returns empty on malformed branch chain") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeNameBranch(0, 4, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::Comment, 1),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	CHECK(candidates.empty());
}

TEST_CASE("Collect rejects dynamic actor-id branches") {
	std::vector<lcf::rpg::EventCommand> commands = {
		MakeCommand(lcf::rpg::EventCommand::Code::EnterHeroName, 0, {4, 0, 0}),
		MakeCommand(lcf::rpg::EventCommand::Code::ConditionalBranch, 0, {5, 4, 1, 0, 1}, "alex"),
		MakeCommand(lcf::rpg::EventCommand::Code::EndBranch, 0),
	};

	auto candidates = NameInputCandidates::Collect(commands, 0, 4);

	CHECK(candidates.empty());
}

TEST_SUITE_END();
