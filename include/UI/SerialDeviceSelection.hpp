/**
 * @file SerialDeviceSelection.hpp
 * @brief "Configure serial connection" window: port and baud rate choice and "Connect" button.
 */
#pragma once
#include <string>
#include <vector>
#include <functional> 
#include "UIElement.hpp"

class UiManager;

/**
 * @brief Home screen window used to start the live acquisition.
 *
 * It does not start anything by itself: when "Connect" is pressed it calls the
 * callback received from UiManager (which configures and starts the SerialService).
 */
class SerialDeviceSelection : public UIElement {
public:
    /// Callback invoked with the chosen port and baud rate.
    using ConnectCallback = std::function<void(const std::string&, int)>;
    /**
     * @brief Creates the window and immediately fills the port list.
     * @param manager           Owning UiManager.
     * @param onConnectCallback Called when "Connect" is pressed with port and baud rate selected.
     */
    explicit SerialDeviceSelection(UiManager* manager, ConnectCallback onConnectCallback);
    /** @brief Draws the two drop-downs and the "Refresh ports" / "Connect" buttons. */
    void draw() override;

private:
    /** @brief Reloads the available ports and baud rates; preselects the first port and 500000 baud. */
    void refreshPorts();

    UiManager* m_uiManager;                  ///< Owner.
    ConnectCallback m_onConnectCallback;     ///< Action of the "Connect" button.

    int m_selectedPortIndex = -1;            ///< Index of the chosen port (-1 = none).
    int m_selectedBaudrateIndex = -1;        ///< Index of the chosen baud rate (-1 = none).
    std::vector<std::string> m_ports;        ///< Available ports.
    std::vector<int> m_baudRates;            ///< Selectable baud rates.
};