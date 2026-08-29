// kfx_sim "creature" cluster, a further depth increment: the first
// individual creature_states_*.c file (not creature_states.c's central
// dispatcher, tested in creature_states_test.cpp) --
// creature_states_tresr.c, the smallest of the per-state files (57
// lines), picked for exactly that reason, the same "cheapest candidate
// first" discipline every stage in this whole plan has followed.
//
// creature_able_to_get_salary() turned out to need pattern B, not just
// pattern A: it calls creature_stats_get_from_thing(), which resolves
// the *effective* model via config_reload_callbacks->get_thing_model(thing)
// -- a real ConfigReloadCallbacks indirection (architecture.md §5.1), not
// a direct thing->model read. The default no-op implementation
// (config.c::config_reload_noop_get_thing_model) always returns 0, which
// would make every test resolve to the same reserved model-0 sentinel
// regardless of what's actually configured -- a fake provider is what
// makes this function's real behavior (pay-based eligibility) testable
// at all, not just a nice-to-have.
#include <catch2/catch_test_macros.hpp>

#include "creature_states_tresr.h"
#include "config_creature.h"
#include "kfx_config_state.h"
#include "thing_data.h"

#include <cstring>

namespace {
ThingModel g_fake_model = 0;
ThingModel fake_get_thing_model(const struct Thing *) { return g_fake_model; }

struct CreatureStatsFixture {
    struct ConfigReloadCallbacks callbacks{};
    struct Thing thing{};

    CreatureStatsFixture() {
        std::memset(&kfx_config_state, 0, sizeof(kfx_config_state));
        kfx_config_state.conf.crtr_conf.model_count = CREATURE_TYPES_MAX;
        g_fake_model = 0;
        callbacks.get_thing_model = fake_get_thing_model;
        set_config_reload_callbacks(&callbacks);
    }
    ~CreatureStatsFixture() { set_config_reload_callbacks(nullptr); } // restores the default no-op table
};
}

TEST_CASE_METHOD(CreatureStatsFixture, "creature_able_to_get_salary is true when the resolved model's pay is nonzero", "[kfx_sim][creature_states_tresr]") {
    kfx_config_state.conf.crtr_conf.model[5].pay = 100;
    g_fake_model = 5;
    CHECK(creature_able_to_get_salary(&thing));
}

TEST_CASE_METHOD(CreatureStatsFixture, "creature_able_to_get_salary is false when the resolved model's pay is zero", "[kfx_sim][creature_states_tresr]") {
    kfx_config_state.conf.crtr_conf.model[5].pay = 0;
    g_fake_model = 5;
    CHECK_FALSE(creature_able_to_get_salary(&thing));
}

TEST_CASE_METHOD(CreatureStatsFixture, "creature_able_to_get_salary is false for the reserved model-0 sentinel, regardless of its pay field", "[kfx_sim][creature_states_tresr]") {
    kfx_config_state.conf.crtr_conf.model[0].pay = 100; // would look eligible if read directly
    g_fake_model = 0;
    CHECK_FALSE(creature_able_to_get_salary(&thing)); // creature_stats_invalid() catches model 0 first
}
