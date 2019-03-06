#include <subsystem/elevator.hpp>

#include <robot.hpp>

#include <garage_math/garage_math.hpp>

namespace garage {
    Elevator::Elevator(std::shared_ptr<Robot>& robot) : Subsystem(robot, "Elevator") {
        ConfigSpeedControllers();
        SetupNetworkTableEntries();
        // TODO think about more
        auto elevator = std::dynamic_pointer_cast<Elevator>(shared_from_this());
        m_RawController = std::make_shared<RawElevatorController>(elevator);
        m_SetPointController = std::make_shared<SetPointElevatorController>(elevator);
        m_VelocityController = std::make_shared<VelocityElevatorController>(elevator);
        m_SoftLandController = std::make_shared<SoftLandElevatorController>(elevator);
    }

    void Elevator::ConfigSpeedControllers() {
        m_ElevatorMaster.ConfigFactoryDefault(CONFIG_TIMEOUT);
        m_ElevatorSlaveOne.ConfigFactoryDefault(CONFIG_TIMEOUT);
        m_ElevatorSlaveTwo.ConfigFactoryDefault(CONFIG_TIMEOUT);
        m_ElevatorSlaveThree.ConfigFactoryDefault(CONFIG_TIMEOUT);

        /* Sensors and Limits */
        m_ElevatorMaster.ConfigSelectedFeedbackSensor(ctre::phoenix::motorcontrol::FeedbackDevice::QuadEncoder, SET_POINT_SLOT_INDEX, CONFIG_TIMEOUT);
        // Soft limit
        m_ElevatorMaster.ConfigForwardSoftLimitThreshold(ELEVATOR_MAX, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigForwardSoftLimitEnable(true, CONFIG_TIMEOUT);
        // Limit switch
        m_ElevatorMaster.ConfigReverseLimitSwitchSource(ctre::phoenix::motorcontrol::LimitSwitchSource_FeedbackConnector,
                                                        ctre::phoenix::motorcontrol::LimitSwitchNormal_NormallyOpen, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigClearPositionOnLimitR(true, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigForwardLimitSwitchSource(ctre::phoenix::motorcontrol::LimitSwitchSource_Deactivated,
                                                        ctre::phoenix::motorcontrol::LimitSwitchNormal_Disabled, CONFIG_TIMEOUT);
        // Brake mode
        m_ElevatorMaster.SetNeutralMode(ctre::phoenix::motorcontrol::NeutralMode::Brake);
        m_ElevatorSlaveOne.SetNeutralMode(ctre::phoenix::motorcontrol::NeutralMode::Brake);
        m_ElevatorSlaveTwo.SetNeutralMode(ctre::phoenix::motorcontrol::NeutralMode::Brake);
        m_ElevatorSlaveThree.SetNeutralMode(ctre::phoenix::motorcontrol::NeutralMode::Brake);
        // Ramping
        m_ElevatorMaster.ConfigOpenloopRamp(ELEVATOR_OPEN_LOOP_RAMP, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigClosedloopRamp(ELEVATOR_CLOSED_LOOP_RAMP, CONFIG_TIMEOUT);
        // Current limiting
        m_ElevatorMaster.ConfigContinuousCurrentLimit(ELEVATOR_CONTINOUS_CURRENT_LIMIT, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigPeakCurrentLimit(ELEVATOR_PEAK_CURRENT_LIMIT, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigPeakCurrentDuration(ELEVATOR_PEAK_CURRENT_DURATION, CONFIG_TIMEOUT);
        // Voltage compensation
        m_ElevatorMaster.ConfigVoltageCompSaturation(ELEVATOR_VOLTAGE_SATURATION, CONFIG_TIMEOUT);
        // Configure following and inversion
        m_ElevatorSlaveOne.Follow(m_ElevatorMaster);
        m_ElevatorSlaveTwo.Follow(m_ElevatorMaster);
        m_ElevatorSlaveThree.Follow(m_ElevatorMaster);
        m_ElevatorMaster.SetInverted(ctre::phoenix::motorcontrol::InvertType::None);
        m_ElevatorSlaveOne.SetInverted(ctre::phoenix::motorcontrol::InvertType::FollowMaster);
        m_ElevatorSlaveTwo.SetInverted(ctre::phoenix::motorcontrol::InvertType::FollowMaster);
        m_ElevatorSlaveThree.SetInverted(ctre::phoenix::motorcontrol::InvertType::FollowMaster);
        // Gains and Motion profiling
        m_ElevatorMaster.ConfigMotionAcceleration(ELEVATOR_ACCELERATION, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigMotionCruiseVelocity(ELEVATOR_VELOCITY, CONFIG_TIMEOUT);
        m_ElevatorMaster.Config_kP(SET_POINT_SLOT_INDEX, ELEVATOR_P, CONFIG_TIMEOUT);
        m_ElevatorMaster.Config_kD(SET_POINT_SLOT_INDEX, ELEVATOR_D, CONFIG_TIMEOUT);
        m_ElevatorMaster.Config_kI(SET_POINT_SLOT_INDEX, ELEVATOR_I, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigMaxIntegralAccumulator(SET_POINT_SLOT_INDEX, ELEVATOR_MAX_I, CONFIG_TIMEOUT);
        m_ElevatorMaster.Config_IntegralZone(SET_POINT_SLOT_INDEX, ELEVATOR_I_ZONE, CONFIG_TIMEOUT);
        m_ElevatorMaster.Config_kF(SET_POINT_SLOT_INDEX, ELEVATOR_F, CONFIG_TIMEOUT);
        // Safety
        m_ElevatorMaster.ConfigClosedLoopPeakOutput(SET_POINT_SLOT_INDEX, 0.5, CONFIG_TIMEOUT);
        m_ElevatorMaster.ConfigAllowableClosedloopError(SET_POINT_SLOT_INDEX, ELEVATOR_ALLOWABLE_CLOSED_LOOP_ERROR, CONFIG_TIMEOUT);

        /* Final enabling */
        m_ElevatorMaster.EnableVoltageCompensation(true);
        m_ElevatorMaster.EnableCurrentLimit(false);
        m_ElevatorMaster.OverrideSoftLimitsEnable(true);
        m_ElevatorMaster.OverrideLimitSwitchesEnable(false);
    }

    void Elevator::SetupNetworkTableEntries() {
        m_Robot->GetNetworkTable()->PutNumber("Elevator/Acceleration", ELEVATOR_ACCELERATION);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/Velocity", ELEVATOR_VELOCITY);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/P", ELEVATOR_P);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/D", ELEVATOR_D);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/F", ELEVATOR_F);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/FF", m_FeedForward);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/I", ELEVATOR_F);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/I Zone", ELEVATOR_F);
        // Add listeners for each entry when a value is updated on the dashboard
        m_Robot->GetNetworkTable()->GetEntry("Elevator/Acceleration").AddListener([&](const nt::EntryNotification& notification) {
            auto acceleration = static_cast<int>(notification.value->GetDouble());
            auto error = m_ElevatorMaster.ConfigMotionAcceleration(acceleration, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator acceleration to %d", acceleration));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/Velocity").AddListener([&](const nt::EntryNotification& notification) {
            auto velocity = static_cast<int>(notification.value->GetDouble());
            auto error = m_ElevatorMaster.ConfigMotionCruiseVelocity(velocity, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator velocity to: %d", velocity));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/F").AddListener([&](const nt::EntryNotification& notification) {
            auto f = notification.value->GetDouble();
            auto error = m_ElevatorMaster.Config_kF(SET_POINT_SLOT_INDEX, f, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator F to: %f", f));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/FF").AddListener([&](const nt::EntryNotification& notification) {
            auto ff = notification.value->GetDouble();
            m_FeedForward = ff;
            Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator FF to: %f", ff));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/D").AddListener([&](const nt::EntryNotification& notification) {
            auto d = notification.value->GetDouble();
            auto error = m_ElevatorMaster.Config_kD(SET_POINT_SLOT_INDEX, d, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator D to: %f", d));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/P").AddListener([&](const nt::EntryNotification& notification) {
            auto p = notification.value->GetDouble();
            m_ElevatorMaster.Config_kP(SET_POINT_SLOT_INDEX, p, 20);
            Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator P to: %f", p));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/I").AddListener([&](const nt::EntryNotification& notification) {
            auto i = notification.value->GetDouble();
            auto error = m_ElevatorMaster.Config_kI(SET_POINT_SLOT_INDEX, i, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator I to: %f", i));
        }, NT_NOTIFY_UPDATE);
        m_Robot->GetNetworkTable()->GetEntry("Elevator/I Zone").AddListener([&](const nt::EntryNotification& notification) {
            auto i_zone = static_cast<int>(notification.value->GetDouble());
            auto error = m_ElevatorMaster.Config_IntegralZone(SET_POINT_SLOT_INDEX, i_zone, CONFIG_TIMEOUT);
            if (error == ctre::phoenix::OK)
                Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format("Changed elevator I Zone to: %d", i_zone));
        }, NT_NOTIFY_UPDATE);
    }

    void Elevator::TeleopInit() {
        m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::PercentOutput, 0.0);
        SetController(nullptr);
        Unlock();
        m_RawController->Reset();
        m_SetPointController->Reset();
        m_VelocityController->Reset();
        m_SoftLandController->Reset();
    }

    void Elevator::UpdateUnlocked(Command& command) {
//        if (command.elevatorSoftLand) {
//            SetController(m_SoftLandController);
//        } else if (math::abs(command.elevatorInput) > DEFAULT_INPUT_THRESHOLD) {
//            SetController(m_RawController);
//        }
        if (m_Controller != m_SoftLandController)
            SetController(m_VelocityController);
        if (m_Controller)
            m_Controller->ProcessCommand(command);
    }

    void Elevator::Update() {
        // TODO add brownout detection and smart current monitoring, check sticky faults
        m_ElevatorMaster.GetStickyFaults(m_StickyFaults);
        if (m_StickyFaults.HasAnyFault()) {
            // Don't log reverse limit switch error
            if ((m_StickyFaults.ToBitfield() & ~(1 << 2)) != 0)
                m_Robot->GetLogger()->Log(lib::LogLevel::k_Error,
                                          m_Robot->GetLogger()->Format("Sticky Faults: %s", m_StickyFaults.ToString().c_str()));
            m_ElevatorMaster.ClearStickyFaults();
        }
        m_EncoderPosition = m_ElevatorMaster.GetSelectedSensorPosition(SET_POINT_SLOT_INDEX);
        m_EncoderVelocity = m_ElevatorMaster.GetSelectedSensorVelocity(SET_POINT_SLOT_INDEX);
        if (m_Controller) {
            m_Controller->Control();
        } else {
            LogSample(lib::LogLevel::k_Warning, "No controller detected");
        }
    }

    void Elevator::SetWantedSetPoint(int wantedSetPoint) {
        SetController(m_SetPointController);
        m_SetPointController->SetWantedSetPoint(wantedSetPoint);
    }

    void Elevator::SpacedUpdate(Command& command) {
        double current = m_ElevatorMaster.GetOutputCurrent(), output = m_ElevatorMaster.GetMotorOutputPercent();
        m_Robot->GetNetworkTable()->PutNumber("Elevator/Encoder", m_EncoderPosition);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/Current", current);
        m_Robot->GetNetworkTable()->PutNumber("Elevator/Output", output);
        Log(lib::LogLevel::k_Info, m_Robot->GetLogger()->Format(
                "Output: %f, Current: %f, Encoder Position: %d, Encoder Velocity: %d",
                output, current, m_EncoderPosition, m_EncoderVelocity));
    }

    bool Elevator::WithinPosition(int targetPosition) {
        return math::withinRange(m_EncoderPosition, targetPosition, ELEVATOR_WITHIN_SET_POINT_AMOUNT);
    }

    bool Elevator::ShouldUnlock(Command& command) {
        return math::absolute(command.elevatorInput) > DEFAULT_INPUT_THRESHOLD;
    }

    void Elevator::OnUnlock() {
        m_Robot->GetLogger()->Log(lib::LogLevel::k_Info, "Unlocked");
    }

    void Elevator::OnLock() {
        m_Robot->GetLogger()->Log(lib::LogLevel::k_Info, "Locked");
    }

    bool Elevator::SetController(std::shared_ptr<ElevatorController> controller) {
        bool different = controller != m_Controller;
        if (different) {
            if (m_Controller) m_Controller->OnDisable();
            m_Controller = controller;
            m_Robot->GetNetworkTable()->PutString("Controller", m_Controller ? m_Controller->GetName() : "None");
            if (controller) controller->OnEnable();
        }
        return different;
    }

    void Elevator::SetRawOutput(double output) {
        SetController(m_RawController);
        m_RawController->SetRawOutput(output);
    }

    void Elevator::SetManual() {
        SetController(m_VelocityController);
    }

    void RawElevatorController::ProcessCommand(Command& command) {
        m_Input = math::threshold(command.elevatorInput, DEFAULT_INPUT_THRESHOLD);
        m_Output = m_Input * 0.2;
    }

    void RawElevatorController::Control() {
        auto elevator = m_Subsystem.lock();
        Log(lib::LogLevel::k_Info, elevator->GetLogger()->Format("Input Value: %f, Output Value: %f", m_Input, m_Output));
        elevator->m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::PercentOutput, m_Output);
    }

    void SetPointElevatorController::ProcessCommand(Command& command) {
        auto elevator = m_Subsystem.lock();
        if (!elevator->m_IsLocked) {
            const double input = math::threshold(command.elevatorInput, DEFAULT_INPUT_THRESHOLD);
            m_WantedSetPoint += static_cast<int>(input * 5000.0);
        }
    }

    void SetPointElevatorController::Control() {
        auto elevator = m_Subsystem.lock();
        m_WantedSetPoint = math::clamp(m_WantedSetPoint, ELEVATOR_MIN, ELEVATOR_MAX);
        Log(lib::LogLevel::k_Info,
            elevator->GetLogger()->Format("Wanted Set Point: %d, Feed Forward: %f", m_WantedSetPoint, elevator->m_FeedForward));
        if (elevator->m_EncoderPosition < ELEVATOR_MAX) {
            elevator->LogSample(lib::LogLevel::k_Info, "Theoretically Okay and Working");
            elevator->m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::MotionMagic,
                                           m_WantedSetPoint,
                                           ctre::phoenix::motorcontrol::DemandType::DemandType_ArbitraryFeedForward,
                                           elevator->m_FeedForward);
        } else {
            elevator->Log(lib::LogLevel::k_Error, "Too High");
            elevator->SetController(elevator->m_SoftLandController);
        }
    }

    void SetPointElevatorController::Reset() {
        m_WantedSetPoint = 0;
    }

    void VelocityElevatorController::ProcessCommand(Command& command) {
        // TODO add too high checking
        m_Input = math::threshold(command.elevatorInput, DEFAULT_INPUT_THRESHOLD);
        m_WantedVelocity = m_Input * ELEVATOR_VELOCITY;
    }

    void VelocityElevatorController::Control() {
        auto elevator = m_Subsystem.lock();
        if (elevator->m_EncoderPosition < ELEVATOR_MAX) {
            Log(lib::LogLevel::k_Info, elevator->GetLogger()->Format("Wanted Velocity: %f", m_WantedVelocity));
            elevator->m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::Velocity,
                                           m_WantedVelocity,
                                           ctre::phoenix::motorcontrol::DemandType::DemandType_ArbitraryFeedForward,
                                           elevator->m_FeedForward);
        } else {
            elevator->Log(lib::LogLevel::k_Error, "Too High");
            elevator->SetController(elevator->m_SoftLandController);
        }
    }

    void VelocityElevatorController::Reset() {
        m_WantedVelocity = 0.0;
    }

    void SoftLandElevatorController::Control() {
        auto elevator = m_Subsystem.lock();
        if (elevator->m_EncoderPosition > SOFT_LAND_ELEVATOR_POSITION_WEAK) {
            elevator->m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::PercentOutput, SAFE_ELEVATOR_DOWN_WEAK);
        } else if (elevator->m_EncoderPosition > SOFT_LAND_ELEVATOR_POSITION_STRONG) {
            elevator->m_ElevatorMaster.Set(ctre::phoenix::motorcontrol::ControlMode::PercentOutput, SAFE_ELEVATOR_DOWN_STRONG);
        }
    }
}