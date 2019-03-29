#pragma once

#include <lib/logger.hpp>

namespace garage {
    struct RobotConfig {
        lib::Logger::LogLevel logLevel = lib::Logger::LogLevel::k_Info;
        bool
                shouldOutput = true,
        // Subsystems
                enableElevator = false,
                enableDrive = true,
                enableFlipper = false,
                enableBallIntake = false,
                enableHatchIntake = false,
                enableOutrigger = false;
        double bottomHatchHeight = 4.7,
        /* Rocket */
        // ==== Ball
                rocketBottomBallHeight = 0.0, rocketMiddleBallHeight = 0.0, rocketTopBallHeight = 0.0,
                rocketBottomBallAngle = 0.0, rocketMiddleBallAngle = 0.0, rocketTopBallAngle = 0.0,
        // ==== Hatch
                rocketMiddleHatchHeight = 53.8, rocketTopHatchHeight = 104.8,
        /* Cargo */
                cargoBallHeightDown = 0.0, cargoBallHeightUp = 0.0,
                cargoBallAngle = 0.0,
        /* Intake */
                groundIntakeBallHeight = bottomHatchHeight, loadingIntakeBallHeight = 0.0,
        /* Climb */
                secondLevelClimbHeight = 10.0, thirdLevelClimbHeight = 20.0;
    };
}
