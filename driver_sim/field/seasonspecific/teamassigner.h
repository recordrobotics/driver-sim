#pragma once

#include <array>

class TeamAssigner
{
  public:
    TeamAssigner() = default;
    ~TeamAssigner() = default;

    TeamAssigner(const TeamAssigner &) = delete;
    TeamAssigner &operator=(const TeamAssigner &) = delete;
    TeamAssigner(TeamAssigner &&) noexcept = default;
    TeamAssigner &operator=(TeamAssigner &&) noexcept = default;

    void update(int allianceStation);

    [[nodiscard]] std::array<uint32_t, 6> getTeamNumbers() const { return teamNumbers; }

  private:
    int lastAllianceStation = 0;
    std::array<uint32_t, 6> teamNumbers = {0, 0, 0, 0, 0, 0};
};