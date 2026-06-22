#include "teamassigner.h"

#include <random>
#include <vector>

#include "../../settings/settingsstore.h"

void TeamAssigner::update(int allianceStation)
{
    if (allianceStation < 1 || allianceStation > 6)
    {
        allianceStation = 1;
    }

    if (allianceStation == lastAllianceStation)
    {
        return;
    }

    lastAllianceStation = allianceStation;

    // Alliance station mapping: 1=red1, 2=red2, 3=red3, 4=blue1, 5=blue2, 6=blue3
    // Indices: 0=first, 1=second, 2=third
    std::vector<uint32_t> pool = settings::gameTeamPool;

    // Place game team
    teamNumbers[allianceStation - 1] = settings::gameTeam;

    // Remove game team from pool
    pool.erase(std::remove(pool.begin(), pool.end(), settings::gameTeam), pool.end());

    // Randomly shuffle with alliance station as seed
    std::mt19937 rng(allianceStation);
    std::shuffle(pool.begin(), pool.end(), rng);

    // Fill team numbers
    size_t pool_idx = 0;
    for (size_t i = 0; i < teamNumbers.size(); ++i)
    {
        if (i != allianceStation - 1 && pool_idx < pool.size())
        {
            teamNumbers[i] = pool[pool_idx++];
        }
    }
}