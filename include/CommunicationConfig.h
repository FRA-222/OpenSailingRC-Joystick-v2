/**
 * @file CommunicationConfig.h
 * @brief Configuration for communication mode selection
 * @author Philippe Hubert
 * @date 2025
 */

#ifndef COMMUNICATION_CONFIG_H
#define COMMUNICATION_CONFIG_H

/**
 * @brief Communication mode enumeration
 */
enum class CommMode {
    ESP_NOW,    ///< ESP-NOW communication (2.4 GHz, short range)
    LORA        ///< LoRa communication (920 MHz, long range)
};

/**
 * @brief Communication configuration class
 */
class CommunicationConfig {
public:
    /**
     * @brief Get current communication mode
     * @return Current CommMode
     */
    static CommMode getMode();
    
    /**
     * @brief Set communication mode
     * @param mode New communication mode
     */
    static void setMode(CommMode mode);
    
    /**
     * @brief Get mode name as string
     * @return Mode name ("ESP-NOW" or "LoRa")
     */
    static const char* getModeName();
    
private:
    static CommMode currentMode;
};

#endif // COMMUNICATION_CONFIG_H
