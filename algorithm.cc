#include "algorithm.h"

std::vector<Mapping> tetris_mappings(const std::vector<Mapping>& all_mappings, const CPUList& occupied_cpus)
{
    /* Get all the mappings that don't overlap with the already occupied CPUs.
     * Consider all the transformed mappings as well (do the TETRiS). */
    std::vector<Mapping> result;

    /* TODO: Do this properly ;) */
    for (const auto& m : all_mappings) {
        for (const auto& equiv_m : m.equivalent_mappings()) {
            if (!occupied_cpus.overlaps_with(equiv_m.cpus))
                result.push_back(equiv_m);
        }
    }

    return result;
}
