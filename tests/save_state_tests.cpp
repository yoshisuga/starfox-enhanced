#include "starfox/assets/rom.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/map_vm.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "save state check failed: " << message << '\n';
        std::exit(1);
    }
}

// A cheap fingerprint of everything the simulation can observably reach:
// the emulated work RAM plus the live object pool. Two simulations that
// agree here have agreed on every byte the game can read back.
[[nodiscard]] std::uint64_t signature(
    const starfox::simulation::GameSimulation& game) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    };
    for (std::uint32_t address = 0x7e0000U; address < 0x7e2000U; ++address) {
        mix(game.map().read_native_byte(address));
    }
    for (const auto handle : game.objects().active_handles()) {
        const auto& object = game.objects().at(handle);
        for (const auto value : {object.world_x, object.world_y,
                 object.world_z}) {
            for (int shift = 0; shift < 32; shift += 8) {
                mix(static_cast<std::uint8_t>(
                    static_cast<std::uint32_t>(value) >> shift));
            }
        }
    }
    return hash;
}

void advance(starfox::simulation::GameSimulation& game, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        static_cast<void>(game.tick(starfox::input::TickInput{}));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: starfox_save_state_tests ROM SYMBOLS\n";
        return 0;  // Nothing to verify without the assembled cartridge.
    }
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);

    starfox::simulation::GameSimulation game{rom, symbols, "LEVEL1_1"};
    advance(game, 40);

    // A clone must be an exact copy of the moment it was taken.
    const auto snapshot = game.clone();
    require(signature(*snapshot) == signature(game),
            "clone did not reproduce the simulation it was taken from");

    // Independence: driving the original must not disturb the snapshot. This
    // is what fails when a copied sub-object still points at the original's
    // object pool, and it is silent corruption rather than a crash.
    const auto snapshot_signature = signature(*snapshot);
    advance(game, 30);
    require(signature(*snapshot) == snapshot_signature,
            "advancing the original mutated the snapshot: a copied sub-object "
            "is still bound to the original");

    // The reference result: what the original reaches after 30 more ticks.
    const auto expected = signature(game);

    // Restoring and replaying the same input must reach the same state.
    game.restore_from(*snapshot);
    require(signature(game) == snapshot_signature,
            "restore did not return the simulation to the snapshot state");
    advance(game, 30);
    require(signature(game) == expected,
            "replay after restore diverged from the uninterrupted run");

    // A clone must itself be runnable, and independent of its parent.
    auto second = snapshot->clone();
    advance(*second, 30);
    require(signature(*second) == expected,
            "a clone did not advance identically to the original");
    require(signature(*snapshot) == snapshot_signature,
            "advancing a clone mutated the snapshot it came from");

    std::cout << "save state round trip verified\n";
    return 0;
}
