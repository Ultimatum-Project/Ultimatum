#include <cassert>
#include <iostream>
#include "web_prompt.h"

// Exercise the actual prompt controller with only the legacy event-loop wait
// hooks stubbed. No game state, UI, or answer validation is mocked.
Controller::Controller(int) {}
Controller::~Controller() {}
void Controller::timerFired() {}
void Controller_startWait() {}
void Controller_endWait() {}

int main() {
    assert(webInteractionDepth() == 0);
    WebPromptController menu("menu", "Menu", "Paused", {{"map", "Unavailable map", false}, {"\033", "Resume"}});
    assert(!menu.stage("map"));
    assert(menu.stage("\033"));
    menu.dispatch();
    assert(menu.cancelled);
    {
        WebInteractionScope interaction;
        assert(webInteractionDepth() == 1);
        { WebInteractionScope nested; assert(webInteractionDepth() == 2); }
        assert(webInteractionDepth() == 1);
    }
    assert(webInteractionDepth() == 0);
    auto buySell = webVendorOptions("bs \r\033", {}, false, true);
    assert(buySell.size() == 3 && buySell[0].label == "Buy" && buySell[1].label == "Sell");
    auto inventory = webVendorOptions("bny", {"Dagger", "Magic Axe", "Mystic Sword"}, false, true);
    assert(inventory[1].label == "Magic Axe" && inventory[2].label == "Mystic Sword");
    auto foodAle = webVendorOptions("fa", {}, false, true);
    assert(foodAle[0].label == "Food" && foodAle[1].label == "Ale");
    auto confirm = webVendorOptions("yn", {}, false, false);
    assert(confirm.size() == 2 && confirm[0].label == "Yes" && confirm[1].label == "No");
    auto nextPage = webVendorOptions(" \r\033", {}, true, true);
    assert(nextPage.size() == 1 && nextPage[0].label == "Continue");
    WebPromptController quantity("amount", "Quantity", "Shop", {{"\033", "Cancel"}}, 2, true);
    assert(!quantity.stage("-1"));
    assert(!quantity.stage("100"));
    assert(!quantity.stage("abc"));
    assert(!quantity.stage(""));
    assert(quantity.stage("12"));
    assert(!quantity.stage("12")); // A second tap must not advance another prompt.
    assert(quantity.getValue().empty()); // Submission is deferred into SDL.
    quantity.dispatch();
    assert(quantity.getValue() == "12");
    assert(!quantity.cancelled);
    quantity.dispatch();
    assert(quantity.getValue() == "12");

    WebPromptController cancel("amount", "Donation", "Gold", {{"\033", "Cancel"}}, 2, true);
    assert(cancel.stage("\033"));
    cancel.dispatch();
    assert(cancel.cancelled);
    assert(cancel.getValue().empty()); // Never becomes an offer of zero gold.
    assert(cancel.generation != quantity.generation);

    WebPromptController question("confirmation", "Question", "Art thou well?", {{"yes", "Yes"}, {"no", "No"}});
    assert(!question.stage("bye"));
    assert(question.keyPressed('N'));
    assert(question.getValue() == "no");
    assert(question.completed);
    assert(!question.stage("yes"));

    WebPromptController shop("choice", "Shop", "Buy or sell?", {{"b", "Buy"}, {"s", "Sell"}, {"\033", "Cancel"}});
    assert(!shop.stage("health"));
    assert(shop.keyPressed('B'));
    assert(shop.getValue() == "b");

    WebPromptController continuation("choice", "Conversation", "A long reply", {{"\r", "Continue"}});
    assert(continuation.keyPressed(' '));
    assert(continuation.getValue() == "\r");

    WebPromptController topic("text", "Conversation", "Your interest?", {{"bye", "Goodbye"}}, 16, true);
    for (char letter : std::string("health")) assert(topic.keyPressed(letter));
    assert(topic.keyPressed(U4_BACKSPACE));
    assert(topic.keyPressed('h'));
    assert(topic.keyPressed(U4_ENTER));
    assert(topic.getValue() == "health");
    assert(topic.completed);

    WebPromptController mantra("text", "Mantra", "Enter thy mantra", {{"\033", "End meditation"}}, 4, true);
    assert(!mantra.stage("toolong"));
    assert(mantra.keyPressed(U4_ESC));
    assert(mantra.cancelled);
    std::cout << "Special prompt controller: quantities, cancellation, choices, keyboard, and duplicate submissions passed\n";
}
