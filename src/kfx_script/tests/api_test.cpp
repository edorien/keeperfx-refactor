// kfx_script: api.c -- the external HTTP API server. Almost everything
// in this file is real sockets/JSON wire protocol, but the event-
// subscription bookkeeping (api_subscribe_event/api_unsubscribe_event/
// api_is_subscribed_to_event/api_clear_all_subscriptions) is pure array
// management over the module's own api_subscriptions[]/api_sub_count,
// with zero socket/game-state involvement -- and get_max_flags() is a
// pure count of a real, statically-populated kfx_game table
// (flag_desc[]). None of get_max_flags/api_clear_all_subscriptions/
// api_is_subscribed_to_event/api_subscribe_event/api_unsubscribe_event/
// api_is_subscribed_to_var/api_subscribe_var/api_unsubscribe_var had any
// header declaration at all despite real external linkage -- all eight
// added to api.h, the usual "add the missing declaration" fix.
//
// The var-subscription cluster (api_is_subscribed_to_var/
// api_subscribe_var/api_unsubscribe_var) additionally reaches into
// kfx_game's get_condition_value() (real Dungeon/Thing/PlayerInfo state
// depending on valtype) and isn't attempted here -- a reasonable next
// increment once that dependency is worth pulling in.
#include <catch2/catch_test_macros.hpp>

#include "api.h"

namespace {
struct ResetSubscriptions {
    ResetSubscriptions() { api_clear_all_subscriptions(); }
};
}

TEST_CASE("get_max_flags counts the real flag_desc[] table and is stable across calls", "[kfx_script][api]") {
    size_t first = get_max_flags();
    CHECK(first > 0);
    CHECK(get_max_flags() == first);
}

TEST_CASE_METHOD(ResetSubscriptions, "api_is_subscribed_to_event is false before any subscription", "[kfx_script][api]") {
    CHECK_FALSE(api_is_subscribed_to_event("onLevelUp"));
}

TEST_CASE_METHOD(ResetSubscriptions, "api_subscribe_event registers a new subscription, observable via api_is_subscribed_to_event", "[kfx_script][api]") {
    CHECK(api_subscribe_event("onLevelUp"));
    CHECK(api_is_subscribed_to_event("onLevelUp"));
    CHECK_FALSE(api_is_subscribed_to_event("onDeath")); // a different, never-subscribed event
}

TEST_CASE_METHOD(ResetSubscriptions, "api_subscribe_event is idempotent for an already-subscribed event", "[kfx_script][api]") {
    CHECK(api_subscribe_event("onLevelUp"));
    CHECK(api_subscribe_event("onLevelUp")); // still true, no duplicate slot consumed
}

TEST_CASE_METHOD(ResetSubscriptions, "api_unsubscribe_event removes a subscription", "[kfx_script][api]") {
    api_subscribe_event("onLevelUp");
    CHECK(api_unsubscribe_event("onLevelUp"));
    CHECK_FALSE(api_is_subscribed_to_event("onLevelUp"));
}

TEST_CASE_METHOD(ResetSubscriptions, "api_unsubscribe_event on an event never subscribed to is a no-op success", "[kfx_script][api]") {
    CHECK(api_unsubscribe_event("neverSubscribed"));
}

TEST_CASE_METHOD(ResetSubscriptions, "multiple distinct event subscriptions coexist independently", "[kfx_script][api]") {
    api_subscribe_event("eventA");
    api_subscribe_event("eventB");
    CHECK(api_is_subscribed_to_event("eventA"));
    CHECK(api_is_subscribed_to_event("eventB"));

    api_unsubscribe_event("eventA");
    CHECK_FALSE(api_is_subscribed_to_event("eventA"));
    CHECK(api_is_subscribed_to_event("eventB")); // unaffected
}

TEST_CASE("api_clear_all_subscriptions removes every active subscription", "[kfx_script][api]") {
    api_subscribe_event("eventA");
    api_subscribe_event("eventB");
    api_clear_all_subscriptions();
    CHECK_FALSE(api_is_subscribed_to_event("eventA"));
    CHECK_FALSE(api_is_subscribed_to_event("eventB"));
}
