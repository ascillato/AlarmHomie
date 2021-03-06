#include <vector>
#include <Homie.h>

#define DPIN_ZONE1 4		// D2 on NodeMCU
#define DPIN_ZONE2 5		// D1 on NodeMCU
#define DPIN_ZONE3 12       // D6 on NodeMCU
#define DPIN_ZONE4 13       // D7 on NodeMCU
#define DPIN_COMP_REF 14    // D5 on NodeMCU

#define RETRIGGER_INTERVAL 10000U // how often a state change can occur (ms)

HomieNode alarmNode("alarm", "multizone-alarm");

class AlarmPin
{
public:
  enum PinState { Unknown, Normal, Triggered, Fault };

  AlarmPin(uint8_t pin, const char* name)
    : _name(name), _pin(pin),
    _aboveLow(0), _aboveHigh(0),
    _lastState(Unknown), _lastStateChange(0), _stateDiffCnt(0)
  {
    // Some pins should already be pulled up to prevent booting to flash mode
    if (pin == 0 || pin == 2)
      pinMode(pin, INPUT);
    else
      pinMode(pin, INPUT_PULLUP);
  }

  void checkLow()
  {
    _aboveLow = digitalRead(_pin);
  }

  void checkHigh()
  {
    _aboveHigh = digitalRead(_pin);
  }

  void update()
  {
    PinState newState;
    if (_aboveHigh)
      newState = PinState::Triggered;  // Above High reference
    else if (_aboveLow)
      newState = PinState::Normal; // Low < x < High
    else
      // Below the Low reference
      // and High < x < Low (Impossible)
      newState = PinState::Fault;

    if (_lastState == PinState::Unknown ||
      ((_lastState != newState) && (millis() - _lastStateChange > RETRIGGER_INTERVAL)))
    {
      ++_stateDiffCnt;
      // Must be in this new state more than once before we consider it a transition
      if (_stateDiffCnt > 1)
      {
        _stateDiffCnt = 0;
        _lastState = newState;
        _lastStateChange = millis();
        Homie.setNodeProperty(alarmNode, _name, PinStateStr(_lastState), false);
      }
    }
    else
      _stateDiffCnt = 0;
  }

  static const char* PinStateStr(PinState p)
  {
    switch (p)
    {
    case PinState::Unknown:
      return "null"; break;
    case PinState::Normal:
      return "normal"; break;
    case PinState::Triggered: 
      return "triggered"; break;
    case PinState::Fault: 
      return "fault"; break;
    }
    return NULL;
  }

private:
  const char* _name;
  uint8_t _pin;
  uint8_t _aboveLow;
  uint8_t _aboveHigh;
  PinState _lastState;
  uint32_t _lastStateChange;
  uint8_t _stateDiffCnt;
};

static std::vector<AlarmPin *> pins;

bool checkAlarm()
{
  const uint32_t UPDATE_INTERVAL = 200U;
  static uint32_t lastUpdate;

  if (millis() - lastUpdate > UPDATE_INTERVAL)
  {
    // Check for above high reference
    for (uint8_t i = 0; i < pins.size(); ++i)
      pins[i]->checkHigh();

    // Turn on the REF transistor to lower reference voltage
    digitalWrite(DPIN_COMP_REF, HIGH);
    delay(1);

    // Check for above low reference
    for (uint8_t i = 0; i < pins.size(); ++i)
    {
      pins[i]->checkLow();
      pins[i]->update();
    }

    // Turn off the REF transistor for the next measurement
    digitalWrite(DPIN_COMP_REF, LOW);
    lastUpdate = millis();
    return true;
  }
  return false;
}


void setupHandler()
{
  digitalWrite(DPIN_COMP_REF, LOW);
  pinMode(DPIN_COMP_REF, OUTPUT);

  pins.push_back(new AlarmPin(DPIN_ZONE1, "zone1"));
  pins.push_back(new AlarmPin(DPIN_ZONE2, "zone2"));
  pins.push_back(new AlarmPin(DPIN_ZONE3, "zone3"));
  pins.push_back(new AlarmPin(DPIN_ZONE4, "zone4"));
}

void loopHandler()
{
  checkAlarm();
}

void setup() {
  // Blink the LED a while to allow the serial port time to connect
  pinMode(BUILTIN_LED, OUTPUT);
  for (unsigned char i=0; i<25; ++i)
  {
    digitalWrite(BUILTIN_LED, LOW);
    delay(100);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(100); 
  }

  Homie.setFirmware("alarm-homie", "1");
  Homie.setSetupFunction(setupHandler);
  Homie.setLoopFunction(loopHandler);
  Homie.registerNode(alarmNode);
  Homie.setup();
}

void loop() {
  Homie.loop();
}
