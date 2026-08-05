#pragma once

namespace ui
{
    enum TransitionState
    {
        TRANSITION_NONE,
        TRANSITION_FADE_TO_BG,
        TRANSITION_FADE_FROM_BG
    };

    class Transition
    {
      public:
        Transition(int startingPage, float duration = 0.30f);

        void transition(int page, bool instant = false);
        void update();
        void draw();

        int getCurrentPage() const { return currentPage; }

      private:
        TransitionState state = TRANSITION_NONE;
        float alpha = 0.0f;
        const float duration;

        int currentPage;
        int targetPage;
    };
} // namespace ui