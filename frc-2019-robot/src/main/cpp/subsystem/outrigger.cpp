#include <subsystem/outrigger.hpp>

#include <robot.hpp>

#include <garage_math/garage_math.hpp>

namespace garage {
    Outrigger::Outrigger(std::shared_ptr<Robot>& robot) : ControllableSubsystem(robot, "Outrigger") {
        m_OutriggerMaster.RestoreFactoryDefaults();
        m_OutriggerSlave.RestoreFactoryDefaults();
        m_OutriggerWheel.RestoreFactoryDefaults();
        m_OutriggerSlave.Follow(m_OutriggerMaster, true);
        m_OutriggerMaster.SetOpenLoopRampRate(OUTRIGGER_RAMPING);
        m_OutriggerWheel.SetOpenLoopRampRate(OUTRIGGER_RAMPING);
        m_OutriggerMaster.SetIdleMode(rev::CANSparkMax::IdleMode::kBrake);
        m_OutriggerSlave.SetIdleMode(rev::CANSparkMax::IdleMode::kBrake);
        m_OutriggerMaster.EnableVoltageCompensation(DEFAULT_VOLTAGE_COMPENSATION);
        m_OutriggerWheel.EnableVoltageCompensation(DEFAULT_VOLTAGE_COMPENSATION);
        // Gains
        m_OutriggerMaster.SetClosedLoopRampRate(OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetP(OUTRIGGER_P, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetI(OUTRIGGER_I, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetD(OUTRIGGER_D, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetIZone(OUTRIGGER_I_ZONE, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetIMaxAccum(OUTRIGGER_MAX_ACCUM, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetFF(OUTRIGGER_FF, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetOutputRange(-1.0, 1.0, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetSmartMotionMaxVelocity(OUTRIGGER_VELOCITY, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetSmartMotionMinOutputVelocity(0.0, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetSmartMotionMaxAccel(OUTRIGGER_ACCELERATION, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetSmartMotionAllowedClosedLoopError(OUTRIGGER_ALLOWABLE_ERROR, OUTRIGGER_SET_POINT_PID_SLOT);
        m_OutriggerController.SetSmartMotionAccelStrategy(rev::CANPIDController::AccelStrategy::kSCurve, OUTRIGGER_SET_POINT_PID_SLOT);
        StopMotors();
    }

    void Outrigger::OnPostInitialize() {
        auto outrigger = WeakFromThis();
        AddController(m_RawController = std::make_shared<RawOutriggerController>(outrigger));
        AddController(m_SetPointController = std::make_shared<SetPointOutriggerController>(outrigger));
    }

    void Outrigger::Reset() {
        ControllableSubsystem::Reset();
        m_Encoder.SetPosition(OUTRIGGER_LOWER);
        StopMotors();
    }

    void Outrigger::StopMotors() {
        m_OutriggerMaster.Set(0.0);
        m_OutriggerWheel.Set(0.0);
    }

    void Outrigger::UpdateUnlocked(Command& command) {
        ControllableSubsystem::UpdateUnlocked(command);
        m_WheelOutput = command.outriggerWheel * 0.1;
    }

    bool Outrigger::ShouldUnlock(Command& command) {
        return std::fabs(command.outrigger) > DEFAULT_INPUT_THRESHOLD ||
               std::fabs(command.outriggerWheel) > DEFAULT_INPUT_THRESHOLD;
    }

    void Outrigger::Update() {
        m_EncoderPosition = m_Encoder.GetPosition();
        m_Angle = math::map(m_EncoderPosition, OUTRIGGER_LOWER, OUTRIGGER_UPPER, OUTRIGGER_STOW_ANGLE, OUTRIGGER_FULL_EXTENDED_ANGLE);
        if (m_Controller) {
            m_Controller->Control();
        } else {
            LogSample(lib::Logger::LogLevel::k_Warning, "No controller detected");
        }
        if (m_Robot->ShouldOutput()) {
            m_OutriggerWheel.Set(m_WheelOutput);
        }
    }

    void Outrigger::SetRawOutput(double output) {
        SetController(m_RawController);
        m_RawController->SetRawOutput(output);
    }

    void Outrigger::SetWantedAngle(double angle) {
        SetController(m_SetPointController);
        m_SetPointController->SetSetPoint(math::map(angle, OUTRIGGER_STOW_ANGLE, OUTRIGGER_FULL_EXTENDED_ANGLE, OUTRIGGER_LOWER, OUTRIGGER_UPPER));
    }

    void Outrigger::SetWheelRawOutput(double output) {
        m_WheelOutput = output;
    }

    bool Outrigger::WithinAngle(double angle) {
        return math::withinRange(m_Angle, angle, OUTRIGGER_WITHIN_ANGLE);
    }

    void SetPointOutriggerController::Control() {
        auto outrigger = m_Subsystem.lock();
        const double feedForward = std::cos(math::d2r(outrigger->m_Angle)) * OUTRIGGER_ANGLE_FF;
        if (outrigger->m_Robot->ShouldOutput()) {
            auto error = outrigger->m_OutriggerController.SetReference(m_SetPoint, rev::ControlType::kSmartMotion,
                                                                       OUTRIGGER_SET_POINT_PID_SLOT,
                                                                       feedForward * DEFAULT_VOLTAGE_COMPENSATION);
            if (error == rev::CANError::kOK) {
//                Log(lib::Logger::LogLevel::k_Debug, lib::Logger::Format("Wanted set point: %f", m_SetPoint));
            } else {
                Log(lib::Logger::LogLevel::k_Error, lib::Logger::Format("CAN Error: %d", error));
            }
        }
    }

    void SetPointOutriggerController::ProcessCommand(Command& command) {
        m_SetPoint += command.outrigger * 0.5;
    }

    void RawOutriggerController::Control() {
        auto outrigger = m_Subsystem.lock();
        if (outrigger->m_Robot->ShouldOutput()) {
            outrigger->m_OutriggerMaster.Set(m_Output);
        }
    }

    void RawOutriggerController::ProcessCommand(Command& command) {
        m_Output = math::clamp(command.elevatorInput, -0.5, 0.5);
    }
}