#pragma once

#include <command.hpp>
#include <robot_config.hpp>
#include <subsystem/drive.hpp>
#include <subsystem/flipper.hpp>
#include <subsystem/elevator.hpp>
#include <subsystem/outrigger.hpp>
#include <subsystem/ball_intake.hpp>
#include <subsystem/hatch_intake.hpp>

#include <lib/logger.hpp>
#include <lib/routine.hpp>
#include <lib/subsystem.hpp>
#include <lib/limelight.hpp>
#include <lib/routine_manager.hpp>

#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>

#include <frc/I2C.h>
#include <frc/Joystick.h>
#include <frc/TimedRobot.h>
#include <frc/XboxController.h>

#include <wpi/optional.h>

#include <chrono>
#include <memory>

namespace garage {
    class Robot : public frc::TimedRobot {
    public:
        enum class LedMode {
            k_Idle = 0, k_NoTarget = 1, k_HasTarget = 2, k_BallIntake = 3, k_Climb = 4
        };

    protected:
        std::shared_ptr<Robot> m_Pointer;
        nt::NetworkTableInstance m_NetworkTableInstance;
        std::shared_ptr<nt::NetworkTable> m_NetworkTable, m_DashboardNetworkTable;
        frc::XboxController m_PrimaryController{0}, m_SecondaryController{1};
        frc::Joystick m_ButtonBoard{2};
        Command m_Command;
        std::shared_ptr<lib::RoutineManager> m_RoutineManager;
        std::shared_ptr<Drive> m_Drive;
        std::shared_ptr<Flipper> m_Flipper;
        std::shared_ptr<Elevator> m_Elevator;
        std::shared_ptr<Outrigger> m_Outrigger;
        std::shared_ptr<BallIntake> m_BallIntake;
        std::shared_ptr<HatchIntake> m_HatchIntake;
        std::vector<std::shared_ptr<lib::Subsystem>> m_Subsystems;
        wpi::optional<std::chrono::system_clock::time_point> m_LastPeriodicTime;
        wpi::optional<std::chrono::system_clock::time_point> m_EndRumble;
        RobotConfig m_Config;
        frc::I2C m_LedModule{frc::I2C::Port::kOnboard, 1};
        LedMode m_LedMode;
        lib::Limelight m_LimeLight;
        std::chrono::milliseconds m_Period;
        // Routines
//        std::shared_ptr<test::TestDriveAutoRoutine> m_DriveForwardRoutine;
        std::shared_ptr<lib::Routine>
                m_TestRoutine,
        // ==== Reset
                m_ResetWithServoRoutine,
        // ==== Utility
                m_GroundBallIntakeRoutine, m_LoadingBallIntakeRoutine, m_PostHatchPlacementRoutine, m_StowFlipperRoutine,
        // ==== End game
                m_EndGameRoutine, m_SecondLevelClimbRoutine, m_ThirdLevelClimbRoutine;

    public:
        void RobotInit() override;

        void CreateRoutines();

        void Reset();

        void RobotPeriodic() override;

        void DisabledInit() override;

        void DisabledPeriodic() override;

        void AutonomousInit() override;

        void AutonomousPeriodic() override;

        void TeleopInit() override;

        void TeleopPeriodic() override;

        void AddSubsystem(std::shared_ptr<lib::Subsystem> subsystem);

        void UpdateCommand();

        void ControllablePeriodic();

        void SetLedMode(LedMode ledMode);

        bool ShouldOutput() const {
            return m_Config.shouldOutput;
        }

        void RumbleControllers() {
            m_EndRumble = std::chrono::system_clock::now() + std::chrono::milliseconds(200);
            SetControllerRumbles(0.4);
        }

        void SetControllerRumbles(double value) {
            // TODO put them in a list then call rumble to make cleaner?
            m_PrimaryController.SetRumble(frc::GenericHID::kLeftRumble, value);
            m_PrimaryController.SetRumble(frc::GenericHID::kRightRumble, value);
            m_SecondaryController.SetRumble(frc::GenericHID::kLeftRumble, value);
            m_SecondaryController.SetRumble(frc::GenericHID::kRightRumble, value);
        }

        RobotConfig& GetConfig() {
            return m_Config;
        }

        lib::Limelight& GetLimelight() {
            return m_LimeLight;
        }

        Command GetLatestCommand() {
            return m_Command;
        }

        template<typename TSubsystem>
        std::shared_ptr<TSubsystem> GetSubsystem() {
            return nullptr;
        }

        std::shared_ptr<NetworkTable> GetNetworkTable() const {
            return m_NetworkTable;
        }

        std::shared_ptr<lib::RoutineManager> GetRoutineManager() {
            return m_RoutineManager;
        }

        void TestInit() override;

        void TestPeriodic() override;
    };

    template<>
    std::shared_ptr<Elevator> Robot::GetSubsystem();

    template<>
    std::shared_ptr<Drive> Robot::GetSubsystem();

    template<>
    std::shared_ptr<Flipper> Robot::GetSubsystem();

    template<>
    std::shared_ptr<BallIntake> Robot::GetSubsystem();

    template<>
    std::shared_ptr<Outrigger> Robot::GetSubsystem();

    template<>
    std::shared_ptr<HatchIntake> Robot::GetSubsystem();
}
