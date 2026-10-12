# Home Assistant integration

Once the ESP32 is adopted (Settings → Devices & services → ESPHome), it exposes a
**button** entity, for example `button.switch_2_waker_wake_switch_2`. Rename it
to something short such as `button.wake_switch2` before you reference it
anywhere.

## Knowing whether the console is awake

The waker only sends; it cannot tell whether the console is on. A ping sensor
is the simplest reliable signal. Give the console a fixed IP (a DHCP
reservation) and add the **Ping** integration for it, or in YAML:

```yaml
binary_sensor:
  - platform: ping
    host: 192.168.1.50   # your Switch 2
    name: Nintendo Switch 2
    count: 2
    scan_interval: 30
```

A sleeping Switch 2 drops off the network after a short while, so the sensor
reports *off* — expect up to a minute of lag in both directions.

## Voice assistants

**Assist** and assistants that support buttons or scenes can press the button
directly ("press Wake Switch 2").

Integrations that only understand on/off devices — such as `emulated_hue` for
Alexa — need a wrapper. A template light works well: *on* presses the button,
the state comes from the ping sensor, and *off* does nothing, because the
console cannot be put to sleep remotely.

The state also reports *on* for 90 seconds after the button is pressed. Voice
assistants read the state right after sending the command, while the ping
sensor needs a few seconds to see the console; without this grace period Alexa
answers *"Nintendo Switch is not responding"* even though the console turns on.

```yaml
template:
  - light:
      - name: Nintendo Switch
        unique_id: switch2_virtual_light
        state: >-
          {% set pressed = states('button.wake_switch2') | as_datetime %}
          {{ is_state('binary_sensor.nintendo_switch_2', 'on')
             or (pressed is not none and now() - pressed < timedelta(seconds=90)) }}
        turn_on:
          - action: button.press
            target:
              entity_id: button.wake_switch2
        turn_off:
          - delay: 0

emulated_hue:
  entities:
    light.switch2_virtual_light:
      name: "Nintendo Switch"
      hidden: false
```

Restart Home Assistant (emulated_hue reads its entity list at startup) and ask
Alexa to discover devices. Then: *"Alexa, turn on Nintendo Switch."*

Prefer a distinctive name: "Switch" alone collides with the generic word for a
light switch in many assistants.

## Automation ideas

Wake the console when the TV switches to its HDMI input:

```yaml
automation:
  - alias: Wake Switch 2 with the TV
    triggers:
      - trigger: state
        entity_id: media_player.living_room_tv
        attribute: source
        to: HDMI 2
    conditions:
      - condition: state
        entity_id: binary_sensor.nintendo_switch_2
        state: "off"
    actions:
      - action: button.press
        target:
          entity_id: button.wake_switch2
```

Or put the button on a dashboard:

```yaml
type: button
entity: button.wake_switch2
name: Switch 2
icon: mdi:nintendo-switch
tap_action:
  action: perform-action
  perform_action: button.press
  target:
    entity_id: button.wake_switch2
```
