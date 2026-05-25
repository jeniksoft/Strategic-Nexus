// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SaveParser.h"

namespace strategic_nexus {

GalaxyState SaveParser::parseSnapshot(const std::filesystem::path& savePath) const
{
    GalaxyState state;
    state.year = 2230;

    EmpireState player;
    player.id = "player";
    player.displayName = "Player Empire";
    player.power.fleetPower = 920000;
    player.power.economicRank = 1;
    player.power.technologyRank = 1;
    player.power.diplomaticInfluence = 65;
    player.personality.boldness = 0.72;
    player.personality.honor = 0.41;

    EmpireState rival;
    rival.id = "empire_12";
    rival.displayName = savePath.empty() ? "Neighboring Coalition" : savePath.stem().string();
    rival.power.fleetPower = 410000;
    rival.power.economicRank = 3;
    rival.power.technologyRank = 4;
    rival.power.diplomaticInfluence = 52;
    rival.personality.paranoia = 0.68;
    rival.personality.opportunism = 0.61;
    rival.adaptiveState.fearOfPlayer = 0.77;

    state.empires.push_back(player);
    state.empires.push_back(rival);
    state.history.push_back({ 2228, "border_war", "player", "empire_12", 0.58 });

    return state;
}

} // namespace strategic_nexus

