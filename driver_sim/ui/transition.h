#pragma once

#include <cstdint>

namespace ui
{
    enum class TransitionState : uint8_t
    {
        None,
        FadeToBackground,
        FadeFromBackground
    };

    class Transition
    {
      public:
        Transition(int startingPage, float duration = 0.30f);

        void transition(int page, bool instant = false);
        void update();
        void draw();

        [[nodiscard]] int getCurrentPage() const { return currentPage; }

      private:
        TransitionState state = TransitionState::None;
        float alpha = 0.0f;
        float duration;

        int currentPage;
        int targetPage;
    };
} // namespace ui