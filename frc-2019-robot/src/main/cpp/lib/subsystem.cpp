#include <lib/subsystem.hpp>

#include <robot.hpp>

#include <utility>
#include <limits>

namespace garage {
    namespace lib {
        Subsystem::Subsystem(std::shared_ptr<Robot>& robot, const std::string& subsystemName)
                : m_Robot(robot), m_SubsystemName(subsystemName) {
            Log(lib::LogLevel::k_Info, "Subsystem Initialized");
        }

        void Subsystem::Lock() {
            OnLock();
            m_IsLocked = true;
        }

        void Subsystem::Unlock() {
            OnUnlock();
            m_IsLocked = false;
        }

        void Subsystem::Periodic() {
            auto command = m_Robot->GetLatestCommand();
            if (m_IsLocked && ShouldUnlock(command))
                Unlock();
            AdvanceSequence();
            if (m_SequenceNumber % SPACED_UPDATE_INTERVAL == 0)
                SpacedUpdate(command);
            Update();
            if (m_IsLocked)
                UpdateLocked();
            else
                UpdateUnlocked(command);
            SetLastCommand(command);
        }

        void Subsystem::AdvanceSequence() {
            if (m_SequenceNumber < std::numeric_limits<unsigned long>::max())
                m_SequenceNumber++;
            else
                m_SequenceNumber = 0;
        }

        void Subsystem::SetLastCommand(Command& command) {
            m_LastCommand = command;
        }

        void Subsystem::Log(lib::LogLevel logLevel, const std::string& log) {
            m_Robot->GetLogger()->Log(logLevel, m_Robot->GetLogger()->Format("[%s] %s", m_SubsystemName.c_str(), log.c_str()));
        }

        void Subsystem::LogSample(lib::LogLevel logLevel, const std::string& log, int frequency) {
            if (m_SequenceNumber % frequency == 0)
                Log(logLevel, log);
        }

        bool Subsystem::ShouldUnlock(Command& command) {
            return true;
        }

        std::shared_ptr<Logger> Subsystem::GetLogger() {
            return m_Robot->GetLogger();
        }

        void Subsystem::AddNetworkTableListener(const std::string& entryName, std::function<bool(const double newValue)> callback) {
            m_Robot->GetNetworkTable()->GetEntry(m_Robot->GetLogger()->Format("%s/%s", m_SubsystemName.c_str(), entryName.c_str())).AddListener(
                    [&](const nt::EntryNotification& notification) {
                        const auto newValue = notification.value->GetDouble();
                        const bool success = callback(newValue);
                        Log(lib::LogLevel::k_Info,
                            m_Robot->GetLogger()->Format("%s %s value %s to %d", success ? "Successfully set" : "Error in setting",
                                                         m_SubsystemName.c_str(), entryName.c_str(), newValue));
                    }, NT_NOTIFY_UPDATE);
        }
    }
}
