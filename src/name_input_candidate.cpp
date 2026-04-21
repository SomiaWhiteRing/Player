/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "name_input_candidate.h"

#include <cctype>
#include <unordered_set>

#include "system.h"

namespace {
	using Code = lcf::rpg::EventCommand::Code;

	bool IsBlankCandidate(std::string_view value) {
		for (size_t index = 0; index < value.size();) {
			auto c = static_cast<unsigned char>(value[index]);
			if (std::isspace(c) != 0) {
				++index;
				continue;
			}
			if (index + 2 < value.size()
					&& c == 0xE3
					&& static_cast<unsigned char>(value[index + 1]) == 0x80
					&& static_cast<unsigned char>(value[index + 2]) == 0x80) {
				index += 3;
				continue;
			}
			return false;
		}

		return true;
	}

	bool IsMatchingActorNameBranch(const lcf::rpg::EventCommand& command, int actor_id) {
		if (static_cast<Code>(command.code) != Code::ConditionalBranch) {
			return false;
		}
		if (command.parameters.size() < 3) {
			return false;
		}
		if (command.parameters[0] != 5 || command.parameters[2] != 1) {
			return false;
		}
		if (command.parameters[1] != actor_id) {
			return false;
		}
		if (command.parameters.size() > 4 && command.parameters[4] != 0) {
			return false;
		}

		return true;
	}

	int FindBranchEnd(const std::vector<lcf::rpg::EventCommand>& commands, int start_index, int indent) {
		for (int index = start_index; index < static_cast<int>(commands.size()); ++index) {
			const auto& command = commands[index];

			if (command.indent < indent) {
				return -1;
			}
			if (command.indent != indent) {
				continue;
			}

			auto code = static_cast<Code>(command.code);
			if (code == Code::EndBranch) {
				return index;
			}
			if (code != Code::ElseBranch) {
				return -1;
			}
		}

		return -1;
	}
}

std::vector<NameInputCandidate> NameInputCandidates::Collect(const std::vector<lcf::rpg::EventCommand>& commands, int enter_hero_name_index, int actor_id) {
	if (enter_hero_name_index < 0 || enter_hero_name_index >= static_cast<int>(commands.size())) {
		return {};
	}

	const auto indent = commands[enter_hero_name_index].indent;
	int index = enter_hero_name_index + 1;

	if (index >= static_cast<int>(commands.size())) {
		return {};
	}

	const auto& first = commands[index];
	if (first.indent != indent || !IsMatchingActorNameBranch(first, actor_id)) {
		return {};
	}

	std::vector<NameInputCandidate> result;
	std::unordered_set<std::string> seen;

	while (index < static_cast<int>(commands.size())) {
		const auto& command = commands[index];
		if (command.indent != indent || !IsMatchingActorNameBranch(command, actor_id)) {
			break;
		}

		auto value = ToString(command.string);
		if (!IsBlankCandidate(value) && seen.insert(value).second) {
			result.push_back({value, value});
		}

		const auto end_index = FindBranchEnd(commands, index + 1, indent);
		if (end_index < 0) {
			return {};
		}
		index = end_index + 1;
	}

	return result;
}
