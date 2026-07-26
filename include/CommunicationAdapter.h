/**
 * @file CommunicationAdapter.h
 * @brief Adapters to make ESP-NOW and LoRa implement ICommunication interface
 * @author Philippe Hubert
 * @date 2025
 */

#ifndef COMMUNICATION_ADAPTER_H
#define COMMUNICATION_ADAPTER_H

#include "ICommunication.h"
#include "ESPNowCommunication.h"
#include "LoRaCommunication.h"

/**
 * @brief Adapter for ESP-NOW communication
 */
class ESPNowAdapter : public ICommunication {
public:
    ESPNowAdapter(ESPNowCommunication& espNow) : espNow(espNow) {}
    
    bool begin() override { return espNow.begin(); }
    void update() override { /* ESP-NOW uses callbacks, no update needed */ }
    bool sendCommand(uint8_t buoyId, const Command& cmd) override {
        return espNow.sendCommand(buoyId, cmd);
    }
    bool hasNewData() override { return espNow.hasNewData(); }
    void clearNewData() override { espNow.clearNewData(); }
    uint8_t getBuoyCount() const override { return espNow.getBuoyCount(); }
    
    BuoyInfo* getBuoyInfo(uint8_t buoyId) override {
        return espNow.getBuoyInfo(buoyId);
    }
    
    const char* getModeName() const override { return "ESP-NOW"; }
    
private:
    ESPNowCommunication& espNow;
};

/**
 * @brief Adapter for LoRa communication
 */
class LoRaAdapter : public ICommunication {
public:
    LoRaAdapter(LoRaCommunication& lora) : lora(lora) {}
    
    bool begin() override { return lora.begin(); }
    void update() override { lora.update(); }
    bool sendCommand(uint8_t buoyId, const Command& cmd) override {
        return lora.sendCommand(buoyId, cmd);
    }
    bool hasNewData() override { return lora.hasNewData(); }
    void clearNewData() override { lora.clearNewData(); }
    uint8_t getBuoyCount() const override { return lora.getBuoyCount(); }
    
    BuoyInfo* getBuoyInfo(uint8_t buoyId) override {
        return lora.getBuoyInfo(buoyId);
    }
    
    const char* getModeName() const override { return "LoRa"; }
    
private:
    LoRaCommunication& lora;
};

#endif // COMMUNICATION_ADAPTER_H
