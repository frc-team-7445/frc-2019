#pragma once

#include <lib/multi_routine.hpp>

namespace garage {
    namespace lib {
        class SequentialRoutine : public MultiRoutine {
        protected:
            unsigned int m_CurrentRoutineIndex;
        public:
            void Update() override;

            bool CheckFinished() override;
        };
    }
}