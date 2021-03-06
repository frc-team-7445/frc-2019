#include <robot.hpp>

#include <routine/climb_hab_routine.hpp>
#include <routine/ball_intake_routine.hpp>
#include <routine/timed_drive_routine.hpp>
#include <routine/lock_flipper_routine.hpp>
#include <routine/reset_with_servo_routine.hpp>
#include <routine/post_hatch_place_routine.hpp>

#include <lib/auto_routine_from_csv.hpp>

#include <test/test_drive_auto_routine.hpp>

#include <frc/DriverStation.h>

namespace garage {
    void Robot::RobotInit() {
        lib::Logger::Log(lib::Logger::LogLevel::k_Info, "Start robot initialization");
        auto begin = std::chrono::high_resolution_clock::now();
        m_Period = std::chrono::milliseconds(std::lround(m_period * 1000.0));
        /* Setup network tables */
        m_NetworkTableInstance = nt::NetworkTableInstance::GetDefault();
        m_NetworkTable = m_NetworkTableInstance.GetTable("Garage Robotics");
        m_DashboardNetworkTable = m_NetworkTable->GetSubTable("Dashboard");
        /* Setup logging system */
        lib::Logger::SetLogLevel(m_Config.logLevel);
        m_NetworkTable->PutNumber("Log Level", static_cast<double>(m_Config.logLevel));
        m_NetworkTable->GetEntry("Log Level").AddListener([&](const nt::EntryNotification& notification) {
            auto logLevel = static_cast<lib::Logger::LogLevel>(std::lround(notification.value->GetDouble()));
            lib::Logger::SetLogLevel(logLevel);
            lib::Logger::Log(lib::Logger::LogLevel::k_Info, lib::Logger::Format("Updated log level to: %d", logLevel));
        }, NT_NOTIFY_UPDATE);
        // Technically this is bad since we are a stack object, but we do not have access to the creation
        // of our class, so this is one of the only ways we can get a shared pointer.
        // It will be destroyed when it goes out of scope but by then everything should be cleaned up.
        m_Pointer = std::shared_ptr<Robot>(this, [](auto robot) {});
        /* Setup routine manager */
        m_RoutineManager = std::make_shared<lib::RoutineManager>(m_Pointer);
        /* Manage subsystems */
        if (m_Config.enableElevator) AddSubsystem(m_Elevator = std::make_shared<Elevator>(m_Pointer));
        if (m_Config.enableDrive) AddSubsystem(m_Drive = std::make_shared<Drive>(m_Pointer));
        if (m_Config.enableFlipper) AddSubsystem(m_Flipper = std::make_shared<Flipper>(m_Pointer));
        if (m_Config.enableBallIntake) AddSubsystem(m_BallIntake = std::make_shared<BallIntake>(m_Pointer));
        if (m_Config.enableHatchIntake) AddSubsystem(m_HatchIntake = std::make_shared<HatchIntake>(m_Pointer));
        if (m_Config.enableOutrigger) AddSubsystem(m_Outrigger = std::make_shared<Outrigger>(m_Pointer));
        /* Create our routines */
        CreateRoutines();
        // Find out how long initialization took and record it
        auto end = std::chrono::high_resolution_clock::now();
        auto initializationTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
        lib::Logger::Log(lib::Logger::LogLevel::k_Info, lib::Logger::Format("End robot initialization, took %d milliseconds", initializationTime));
    }

    void Robot::CreateRoutines() {
//        // Render drive trajectory in initialization because it takes a couple of seconds
//        m_DriveForwardRoutine = std::make_shared<test::TestDriveAutoRoutine>(m_Pointer, "Test Drive");
//        m_DriveForwardRoutine->CalculatePath();
        m_ResetWithServoRoutine = std::make_shared<ResetWithServoRoutine>(m_Pointer);
        /* Utility routines */
        m_GroundBallIntakeRoutine = std::make_shared<BallIntakeRoutine>(m_Pointer, m_Config.groundIntakeBallHeight, FLIPPER_UPPER_ANGLE);
        m_LoadingBallIntakeRoutine = std::make_shared<BallIntakeRoutine>(m_Pointer, m_Config.loadingIntakeBallHeight, FLIPPER_UPPER_ANGLE);
        m_PostHatchPlacementRoutine = std::make_shared<PostHatchPlaceRoutine>(m_Pointer);
        /* End game routines */
        m_EndGameRoutine = std::make_shared<LockFlipperRoutine>(m_Pointer);
        m_SecondLevelClimbRoutine = std::make_shared<ClimbHabRoutine>(m_Pointer, m_Config.secondLevelClimbHeight);
        m_ThirdLevelClimbRoutine = std::make_shared<ClimbHabRoutine>(m_Pointer, m_Config.thirdLevelClimbHeight);
        m_StowFlipperRoutine = std::make_shared<ElevatorAndFlipperRoutine>(m_Pointer, 0.0, 70.0);
//        // Testing routines
//        auto
//                testWaitRoutineOne = std::make_shared<lib::WaitRoutine>(m_Pointer, 500l),
//                testWaitRoutineTwo = std::make_shared<lib::WaitRoutine>(m_Pointer, 1000l);
//        m_TestRoutine = std::make_shared<lib::ParallelRoutine>
//                (m_Pointer, "Test Routine", lib::RoutineVector{testWaitRoutineOne, testWaitRoutineTwo});
//        m_TestRoutine = std::make_shared<SetFlipperAngleRoutine>(m_Pointer, 90.0, "Meme");
//        m_TestRoutine = std::make_shared<lib::AutoRoutineFromCSV>(m_Pointer, "start_to_middle_left_hatch", "Start To Middle Hatch");
//        m_TestRoutine->PostInitialize();
//        m_TestRoutine = std::make_shared<TimedDriveRoutine>(m_Pointer, 1000l, 0.1, "Meme");
    }

    void Robot::AddSubsystem(std::shared_ptr<lib::Subsystem> subsystem) {
        m_Subsystems.push_back(subsystem);
        subsystem->PostInitialize();
    }

    void Robot::RobotPeriodic() {}

    void Robot::DisabledInit() {
        m_LimeLight.SetLedMode(lib::Limelight::LedMode::k_Off);
        SetLedMode(LedMode::k_Idle);
    }

    void Robot::DisabledPeriodic() {}

    void Robot::AutonomousInit() {
        Reset();
    }

    void Robot::AutonomousPeriodic() {
        ControllablePeriodic();
    }

    void Robot::Reset() {
        m_LimeLight.SetLedMode(lib::Limelight::LedMode::k_Off);
        SetLedMode(LedMode::k_Idle);
        m_LastPeriodicTime.reset();
        m_Command = {};
        m_RoutineManager->Reset();
        for (const auto& subsystem : m_Subsystems) {
            subsystem->Reset();
        }
    }

    void Robot::TeleopInit() {
        Reset();
    }

    void Robot::ControllablePeriodic() {
        // See if we are taking too much time and not getting fifty updates a second
        auto now = std::chrono::system_clock::now();
        if (m_LastPeriodicTime) {
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastPeriodicTime.value());
            if (delta > m_Period * 1.05) {
                lib::Logger::Log(lib::Logger::LogLevel::k_Warning,
                                 lib::Logger::Format("Loop was more than five percent of expected, took %d milliseconds", delta));
            }
        }
        m_LastPeriodicTime = now;
        UpdateCommand();
        if (m_EndRumble && std::chrono::system_clock::now() >= m_EndRumble) {
            SetControllerRumbles(0.0);
            m_EndRumble.reset();
        }
        m_RoutineManager->AddRoutinesFromCommand(m_Command);
        m_RoutineManager->Update();
        for (const auto& subsystem : m_Subsystems) {
            subsystem->Periodic();
        }
//        m_DashboardNetworkTable->PutNumber("Match Time Remaining", frc::DriverStation::GetInstance().GetMatchTime());
    }

    void Robot::TeleopPeriodic() {
        ControllablePeriodic();
    }

    void Robot::UpdateCommand() {
        /* Routines */
        m_Command.routines.clear();
        if (m_PrimaryController.GetBackButtonPressed() || m_SecondaryController.GetBackButtonPressed()) {
            m_RoutineManager->TerminateAllRoutines();
        }
        if (m_PrimaryController.GetStartButtonPressed() || m_SecondaryController.GetStartButtonPressed()) {
            m_Command.routines.push_back(m_StowFlipperRoutine);
//            m_Command.routines.push_back(m_TestRoutine);
//            m_Command.offTheBooksModeEnabled = !m_Command.offTheBooksModeEnabled;
//            if (m_Command.offTheBooksModeEnabled) {
//                m_Command.routines.push_back(m_EndGameRoutine);
//            } else {
//                m_Command.routines.push_back(m_ResetWithServoRoutine);
//            }
        }
        /* Four buttons */
        if (m_Drive) {
            if (m_PrimaryController.GetAButton() || m_SecondaryController.GetAButton()) {
                if (m_LimeLight.HasTarget()) {
                    m_Drive->AutoAlign();
                    SetLedMode(LedMode::k_HasTarget);
                } else {
                    SetLedMode(LedMode::k_NoTarget);
                }
            }
            if (m_PrimaryController.GetAButtonReleased() || m_SecondaryController.GetAButtonReleased()) {
                m_Drive->Unlock();
                SetLedMode(LedMode::k_Idle);
                m_LimeLight.SetLedMode(lib::Limelight::LedMode::k_Off);
            }
            if (m_PrimaryController.GetAButtonPressed() || m_SecondaryController.GetAButtonPressed()) {
                m_LimeLight.SetLedMode(lib::Limelight::LedMode::k_On);
            }
        }
        if (m_PrimaryController.GetBButtonPressed() || m_SecondaryController.GetBButtonPressed()) {
            m_Command.routines.push_back(m_GroundBallIntakeRoutine);
        }
        if (m_Flipper) {
            if (m_PrimaryController.GetXButtonPressed() || m_SecondaryController.GetXButtonPressed()) {
                m_Flipper->SetAngle(m_Flipper->GetAngle() > FLIPPER_STOW_ANGLE ? FLIPPER_LOWER_ANGLE : FLIPPER_UPPER_ANGLE);
            }
        }
        const int primaryPOV = m_PrimaryController.GetPOV(), secondaryPOV = m_SecondaryController.GetPOV();
        const bool
        // ==== Button button
                elevatorDown = primaryPOV == 180 || secondaryPOV == 180 || secondaryPOV == 135 || secondaryPOV == 225,
        // ==== Top button
                elevatorStow = primaryPOV == 0 || secondaryPOV == 0 || secondaryPOV == 45 || secondaryPOV == 315,
                modButton = secondaryPOV == 90;
        /* DPad */
        if (m_Elevator) {
            // TODO hash map maybe?
            if (elevatorDown) {
                m_Elevator->SetWantedSetPoint(0.0);
            } else if (elevatorStow || m_ButtonBoard.GetRawButtonPressed(7)) {
                m_Elevator->SetWantedSetPoint(m_Config.bottomHatchHeight);
            } else if (m_ButtonBoard.GetRawButtonPressed(1)) {
                m_Elevator->SetWantedSetPoint(m_Config.rocketMiddleHatchHeight);
            } else if (m_ButtonBoard.GetRawButtonPressed(2)) {
                m_Elevator->SetWantedSetPoint(m_Config.rocketTopHatchHeight);
            } else if (m_ButtonBoard.GetRawButtonPressed(8)) {
                m_Elevator->SetWantedSetPoint(m_Config.rocketBottomBallHeight);
            } else if (m_ButtonBoard.GetRawButtonPressed(5)) {
                m_Elevator->SetWantedSetPoint(m_Config.rocketMiddleBallHeight);
            } else if (m_ButtonBoard.GetRawButtonPressed(4)) {
                m_Elevator->SetWantedSetPoint(m_Config.rocketTopBallHeight);
            }
        }
        const bool secondaryY = m_SecondaryController.GetYButtonPressed();
        m_Command.hatchIntakeDown = m_PrimaryController.GetYButtonPressed() || (secondaryY && !modButton);
        if (secondaryY && modButton) {
            m_Command.routines.push_back(m_PostHatchPlacementRoutine);
        }
        double wantedAngle = m_Flipper ? m_Flipper->GetWantedAngle() : FLIPPER_UPPER_ANGLE;
        const bool shouldInvertDrive = wantedAngle < FLIPPER_STOW_ANGLE;
        /* Joysticks */
        m_Command.driveForward = math::threshold(-m_PrimaryController.GetY(frc::GenericHID::JoystickHand::kRightHand), DEFAULT_INPUT_THRESHOLD);
        m_Command.driveTurn = math::threshold(m_PrimaryController.GetX(frc::GenericHID::JoystickHand::kRightHand), DEFAULT_INPUT_THRESHOLD);
        m_Command.driveForward += math::threshold(-m_SecondaryController.GetY(frc::GenericHID::kRightHand), XBOX_360_STICK_INPUT_THRESHOLD) * 0.25;
        m_Command.driveTurn += math::threshold(m_SecondaryController.GetX(frc::GenericHID::kRightHand), XBOX_360_STICK_INPUT_THRESHOLD) * 0.25;
//        const auto px = m_PrimaryController.GetX(frc::GenericHID::kRightHand), sx = m_SecondaryController.GetX(frc::GenericHID::kRightHand);
//        lib::Logger::Log(lib::Logger::LogLevel::k_Info, lib::Logger::Format("%f, %f, %f, %f, %f", px, math::threshold(px, 0.225), sx, math::threshold(sx, 0.225), m_Command.driveTurn));
        // When we are flipped over invert the driving controls to make it feel natural
        if (shouldInvertDrive) {
            m_Command.driveForward *= -1;
        }
        m_Command.elevatorInput = math::threshold(-m_PrimaryController.GetY(frc::GenericHID::kLeftHand), DEFAULT_INPUT_THRESHOLD);
        m_Command.elevatorInput += math::threshold(-m_SecondaryController.GetY(frc::GenericHID::kLeftHand), XBOX_360_STICK_INPUT_THRESHOLD) * 0.7;
        /* Triggers */
        double triggers = math::threshold(
                m_SecondaryController.GetTriggerAxis(frc::GenericHID::JoystickHand::kRightHand) -
                m_SecondaryController.GetTriggerAxis(frc::GenericHID::JoystickHand::kLeftHand), DEFAULT_INPUT_THRESHOLD);
        triggers = math::clamp(triggers, -1.0, 1.0);
        m_Command.isQuickTurn = m_PrimaryController.GetTriggerAxis(frc::GenericHID::kLeftHand) > 0.35;
        /* Bumpers */
        auto bumpers = math::axis<double>(
                m_PrimaryController.GetBumper(frc::GenericHID::JoystickHand::kRightHand),
                m_PrimaryController.GetBumper(frc::GenericHID::JoystickHand::kLeftHand));
        bumpers += math::axis<double>(
                m_SecondaryController.GetBumper(frc::GenericHID::JoystickHand::kRightHand),
                m_SecondaryController.GetBumper(frc::GenericHID::JoystickHand::kLeftHand));
        bumpers = math::clamp(bumpers, -1.0, 1.0);
        /* Off the books */
//        if (m_ButtonBoard.GetRawButtonPressed(6)) {
//            m_Command.routines.push_back(m_SecondLevelClimbRoutine);
//        } else if (m_ButtonBoard.GetRawButtonPressed(3)) {
//            m_Command.routines.push_back(m_ThirdLevelClimbRoutine);
//        }
        if (m_Command.offTheBooksModeEnabled) {
            m_Command.outrigger = triggers;
            m_Command.outriggerWheel = bumpers;
            m_Command.ballIntake = 0;
            m_Command.flipper = 0;
        } else {
            m_Command.outrigger = 0;
            m_Command.outriggerWheel = 0;
            m_Command.ballIntake = bumpers;
            m_Command.flipper = triggers;
        }
    }

    void Robot::TestInit() {
        Reset();
    }

    void Robot::TestPeriodic() {
        ControllablePeriodic();
    }

    void Robot::SetLedMode(Robot::LedMode ledMode) {
        if (ledMode != m_LedMode) {
            m_LedMode = ledMode;
            auto ledModeInt = static_cast<uint8_t>(ledMode);
            // Send one byte of data over I2C to Arduino, setting the LED type
            m_LedModule.Transaction(&ledModeInt, 1, nullptr, 0);
        }
    }

    template<>
    std::shared_ptr<Elevator> Robot::GetSubsystem() {
        return m_Elevator;
    }

    template<>
    std::shared_ptr<Drive> Robot::GetSubsystem() {
        return m_Drive;
    }

    template<>
    std::shared_ptr<Flipper> Robot::GetSubsystem() {
        return m_Flipper;
    }

    template<>
    std::shared_ptr<BallIntake> Robot::GetSubsystem() {
        return m_BallIntake;
    }

    template<>
    std::shared_ptr<Outrigger> Robot::GetSubsystem() {
        return m_Outrigger;
    }

    template<>
    std::shared_ptr<HatchIntake> Robot::GetSubsystem() {
        return m_HatchIntake;
    }
}

#ifndef RUNNING_FRC_TESTS

int main() {
    return frc::StartRobot<garage::Robot>();
}

#endif
