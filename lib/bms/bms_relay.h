#ifndef BMS_RELAY_H
#define BMS_RELAY_H

#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "filter.h"
#include "packet_tracker.h"

class Packet;

class BmsRelay {
 public:
  /**
   * @brief Function polled for new byte from BMS. Expected to return negative
   * value when there's no data available on the wire.
   */
  typedef std::function<int()> Source;
  /**
   * @brief Function called to send data to the MB.
   */
  typedef std::function<void(uint8_t)> Sink;

  /**
   * @brief Packet callback.
   */
  typedef std::function<void(BmsRelay*, Packet*)> PacketCallback;

  typedef std::function<unsigned long()> Millis;

  BmsRelay(const Source& source, const Sink& sink, const Millis& millis);

  /**
   * @brief Call from arduino loop.
   */
  void loop();

  void addReceivedPacketCallback(const PacketCallback& callback) {
    receivedPacketCallbacks_.push_back(callback);
  }

  void addForwardedPacketCallback(const PacketCallback& callback) {
    forwardedPacketCallbacks_.push_back(callback);
  }

  void setPowerOffCallback(const std::function<void(void)>& c) {
    powerOffCallback_ = c;
  }

  void setSocRewriterCallback(const std::function<int8_t(int8_t, bool*)>& c) {
    socRewriterCallback_ = c;
  }

  void setUnknownDataCallback(const Sink& c) { unknownDataCallback_ = c; }

  /**
   * @brief If set to non-zero value, spoofs captured BMS serial
   * with the number provided here. The serial number can be found
   * on the sticker on the bottom side of BMS. The lower number on
   * the sticker without the 4 leading (most significant) digits
   * goes here.
   */
  void setBMSSerialOverride(uint32_t serial) { serial_override_ = serial; }

  /**
   * @brief Get the captured BMS serial number.
   *
   * @return non-zero serial, if it's already been captured.
   */
  uint32_t getCapturedBMSSerial() { return captured_serial_; }

  /**
   * @brief Toggle to drop BMS status or not.
   */
  void dropBMSStatus(bool drop) { drop_status_ = drop; }

  /**
   * @brief Battery percentage as reported by the BMS.
   */
  int8_t getBmsReportedSOC() { return bms_soc_percent_; }

  /**
   * @brief Spoofed battery percentage sent to the controller.
   */
  int8_t getOverriddenSOC() { return overridden_soc_percent_; }
  /**
   * @brief Cell voltages in millivolts.
   * @return pointer to a 15 element array.
   */
  uint16_t* const getCellMillivolts() { return cell_millivolts_; }

  uint16_t getTotalVoltageMillivolts() { return total_voltage_millivolts_; }

  /**
   * @brief Cell voltages in millivolts.
   * @return pointer to a 5 element array.
   */
  int8_t* const getTemperaturesCelsius() { return temperatures_celsius_; }

  uint8_t getAverageTemperatureCelsius() { return avg_temperature_celsius_; }

  /**
   * @brief Current In Amps.
   */
  float getCurrentInAmps() { return current_ * CURRENT_SCALER; }

  int32_t getUsedChargeMah() {
    return current_times_milliseconds_used_ * CURRENT_SCALER / 3600;
  }
  int32_t getRegeneratedChargeMah() {
    return current_times_milliseconds_regenerated_ * CURRENT_SCALER / 3600;
  }

  bool isCurrent() {return last_status_byte_ & 0x5; }
  bool isCharging() { return last_status_byte_ & 0x20; }
  bool isBatteryEmpty() { return last_status_byte_ & 0x4; }
  bool isBatteryTempOutOfRange() {
    // Both these bits throw the same error, one is prob hot and the other is
    // cold.
    return last_status_byte_ & 3;
  }
  bool isBatteryOvercharged() { return last_status_byte_ & 8; }

  const PacketTracker& getPacketTracker() const { return packet_tracker_;}

 private:
  static constexpr float CURRENT_SCALER = 0.055;
  void processNextByte();
  void purgeUnknownData();
  void maybeReplayPackets();
  void ingestPacket(Packet& p);

  std::vector<PacketCallback> receivedPacketCallbacks_;
  std::vector<PacketCallback> forwardedPacketCallbacks_;
  Sink unknownDataCallback_;
  std::function<int8_t(int8_t, bool*)> socRewriterCallback_;
  std::function<void(void)> powerOffCallback_;

  std::vector<uint8_t> sourceBuffer_;
  uint32_t serial_override_ = 0;
  uint32_t captured_serial_ = 0;
  bool drop_status_ = true;
  int16_t current_ = 0;

  int8_t bms_soc_percent_ = -1;
  int8_t overridden_soc_percent_ = -1;
  uint16_t cell_millivolts_[15] = {0};
  uint16_t total_voltage_millivolts_ = 0;
  LowPassFilter lowest_cell_voltage_filter_;
  uint16_t filtered_lowest_cell_voltage_millivolts_ = 0;

  int8_t temperatures_celsius_[5] = {0};
  int8_t avg_temperature_celsius_ = 0;
  unsigned long last_current_message_millis_ = 0;
  int16_t last_current_ = 0;
  int32_t current_times_milliseconds_used_ = 0;
  int32_t current_times_milliseconds_regenerated_ = 0;
  uint8_t last_status_byte_ = 0;
  const Source source_;
  const Sink sink_;
  const Millis millis_;
  PacketTracker packet_tracker_;

  void bmsStatusParser(Packet& p);
  void bmsSerialParser(Packet& p);
  void currentParser(Packet& p);
  void batteryPercentageParser(Packet& p);
  void cellVoltageParser(Packet& p);
  void temperatureParser(Packet& p);
  void powerOffParser(Packet& p);
};

#endif  // BMS_RELAY_H