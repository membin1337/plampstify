# Soil moisture sensor — DIY capacitive probe (design doc)

Design only - nothing in this doc is implemented yet. Written up first per
request, before touching firmware or hardware, so the approach can be
reviewed/adjusted before any wiring or code happens.

## 1. Problem

A bought "cheap" soil moisture sensor died within a month. Almost
certainly one of two known failure modes:

- **Resistive probes** (bare two-prong boards, e.g. "YL-69" style) corrode
  from electrolysis - current passes directly through the soil between two
  exposed metal prongs, and that current physically degrades the metal.
  Fertilized/salty soil (this setup uses Top Crop nutrients - see
  `feedingSchedule.js` in plampControlCenter) accelerates this further.
- **Cheap "capacitive" boards** often fail differently: the sensing
  principle itself doesn't pass current through soil, but the factory
  conformal coating rarely seals the cut PCB edge properly, so moisture
  wicks in and corrodes the exposed copper there anyway.

Both failure modes share a root cause: **metal in direct electrical
contact with wet/salty soil, insufficiently protected.** The fix isn't a
better metal - it's removing that contact entirely.

## 2. Why capacitive + fully potted

A capacitive probe senses soil moisture through its effect on the
dielectric constant between two plates, not by conducting current through
the soil. If the plates are **fully encapsulated in epoxy** (no exposed
metal at all, not even at a cut edge), there is no electrical path for
corrosion to start on - the probe senses through the epoxy/soil dielectric
without ever touching it electrically. This is the design goal: not
"slower corrosion," but no corrosion path in the first place.

## 3. Design option A (recommended): 555-oscillator capacitive probe

This is how commercial capacitive soil sensors actually work internally -
the difference here is potting it properly.

### 3.1 How it works

- An NE555 timer in astable mode oscillates at a frequency set by its
  RC/RC-and-probe-capacitance network. The probe's two plates form (or
  feed into) that timing capacitance - wetter soil raises the effective
  dielectric constant between the plates, which shifts the oscillator's
  frequency/duty cycle.
- The oscillator's square-wave output is fed through a simple diode +
  smoothing-capacitor rectifier (a peak/average detector), producing a
  smooth analog DC voltage that rises and falls with moisture.
- The ESP32 reads that DC voltage on a normal ADC-capable GPIO -
  electrically no different from how `sensors.cpp` already reads the
  DHT22, just a different pin and a linear (not DHT-protocol) conversion.

### 3.2 Bill of materials

| Part | Notes |
|---|---|
| NE555 timer IC | Any standard 555, through-hole is fine |
| 2x resistors | e.g. R1 ≈ 1MΩ, R2 ≈ 100kΩ to start - tune on a breadboard for a convenient oscillation range, not safety-critical |
| 1x small fixed capacitor | 100pF-1nF, sets the oscillator's baseline range alongside the probe's own capacitance |
| 1x signal diode | 1N4148, for the output rectifier |
| 1x smoothing capacitor | 1-10µF, rectifier output smoothing |
| 1x bleed resistor | ~100kΩ, rectifier output discharge path |
| 2x copper plates | Copper tape or copper-clad FR4, ~20mm x 50mm each |
| 1x insulating spacer | Thin acrylic/FR4/rigid plastic, ~1-2mm thick, cut to the same size as the plates, sandwiched between them |
| 2-conductor silicone wire | Plate leads + power/ground run back to the ESP32; silicone jacket handles direct burial better than PVC |
| 2-part waterproof epoxy | e.g. JB Weld ClearWeld or equivalent, for potting the plate assembly |
| Heat-shrink tubing | Optional extra seal where the leads exit the epoxy |
| Small enclosure | For the 555 circuit itself - stays above soil, only the plates get buried |

### 3.3 Plate geometry

- **Plate size**: ~20mm x 50mm each - roughly matches commercial
  capacitive sensor dimensions, a reasonable balance of signal strength
  vs. probe bulk.
- **Plate spacing**: 1-2mm, set by the insulating spacer between them.
  Thinner gap = stronger capacitive coupling/sensitivity, but the
  assembly has to stay rigid and fully sealed at that thickness.
- **Insertion depth**: 5-8cm into the root zone - deep enough to read
  root-zone moisture rather than just surface soil, which dries out much
  faster and would trigger false "needs watering" alerts.

### 3.4 Potting / waterproofing procedure

1. Solder plate leads first; verify continuity and no shorts before
   potting anything (nothing is fixable once epoxied).
2. Sandwich the two plates around the insulating spacer, tack in place.
3. Dip/pot the **entire** plate assembly - both faces and every edge -
   in epoxy. Only the lead wires should exit the cured epoxy block.
4. Full cure per the epoxy's spec sheet (commonly ~24h).
5. Optional extra seal: heat-shrink over the epoxy/wire seam, with a dab
   of epoxy at the very top so water can't wick down inside the wire's
   own insulation jacket into the circuit above soil.
6. Before burying it: verify with a multimeter that there's no continuity
   between either plate lead and a wet paper towel wrapped around the
   cured probe. This is the actual proof the potting worked.

## 4. Design option B (simpler, fewer parts): ESP32 native touch-pin sensing

The ESP32 has a built-in capacitive touch peripheral (`touchRead()`,
normally used for touch buttons) that can be repurposed to sense a
potted probe's capacitance directly - no 555 circuit or passive
components needed at all.

- Probe: same fully-potted plate construction as option A (section 3.3-
  3.4), but with a single lead wired straight to a touch-capable GPIO
  instead of into a 555 circuit.
- Touch-capable pins not already spoken for in this project: T8
  (**GPIO33**) and T9 (GPIO32) are free. T7 (GPIO27) is already earmarked
  for the DHT22's switchable VCC in `README.md`'s wiring proposal, and T0
  (GPIO4) is the DHT22 data pin - avoid both. **GPIO33 is the
  recommendation** if going this route.
- Tradeoff: much faster to build (zero extra ICs/passives), but
  `touchRead()` values are more sensitive to lead length/routing noise
  and drift with ambient temperature, so it needs more careful
  calibration and is less proven for unattended long-term monitoring
  than the 555 approach.

**Recommendation: build option A.** It's the well-documented, predictable
path (it's literally what the commercial sensor already used, just sealed
properly this time). Option B is worth keeping in mind as a fast way to
validate the plate geometry/potting technique on a breadboard before
committing to etching/soldering the 555 circuit, not as the final design.

## 5. Firmware plan (plampstify)

Follows the same modular pattern the recent refactor established
(`sensors.cpp`/`actuators.cpp`/`automation.cpp`, one concern per file,
`main.cpp` stays thin).

### 5.1 New module: `soil_moisture.{h,cpp}`

- `initSoilMoisture()` - configure the ADC pin (option A) or touch pin
  (option B); load the two calibration points (see below) from
  `Preferences` (new namespace, e.g. `"soil"`).
- `pollSoilMoisture()` - same shape as `sensors.cpp`'s `pollSensors()`:
  read on an interval, track consecutive failures/staleness the same way.
- `getSoilMoisturePercent()`, `isSoilMoistureHealthy()`,
  `getSoilMoistureLastReadAgeMs()` - mirrors the existing DHT getters.
- **Calibration**: raw ADC/touch values are meaningless on their own and
  differ probe-to-probe, so store two reference points in `Preferences` -
  `dryRaw` (probe in open air or bone-dry soil) and `wetRaw` (probe fully
  submerged in water) - and linearly interpolate (clamped to 0-100%)
  between them for the reported percentage. Add a small calibration
  routine (triggered via a new HTTP endpoint or a serial command) to
  (re)capture these two points rather than hardcoding guessed values.

### 5.2 New HTTP route: `GET /sensors/soil/read`

Same shape as the existing `/sensors/1/read`:

```json
{ "sensorId": "soil1", "moisturePercent": "42.30", "raw": 1820, "timestamp": "..." }
```

Also fold a `soil` field into the existing `/status` response (same
pattern as the `sensor` array already there) so `poller.js` picks it up
in the single request per cycle it already makes, instead of adding a
second round trip against the ESP32's limited connection pool.

## 6. plamp-api integration (plampControlCenter/server)

- Add a nullable `soil_moisture` column to the existing `sensor_readings`
  hypertable rather than a second table - one write per poll cycle,
  same as temperature/humidity today, no extra join needed to chart it
  alongside them.
- `poller.js`: insert `soil_moisture` alongside temperature/humidity from
  the same `/status` response.
- New alert bands, same active/cleared transition pattern as the pH
  alerts added recently: a configurable "needs watering" threshold
  (e.g. below 30% → new code, `SOIL_TOO_DRY`) and a "waterlogged"
  threshold (e.g. above 80% → `SOIL_TOO_WET`), both editable via
  `/settings` the same way temp/humidity thresholds are today.

## 7. Frontend integration (plampControlCenter)

- **Dashboard**: a third stat card (Soil Moisture) alongside
  Temperature/Humidity, same visual language as the existing cards.
- **Environment page**: a third area chart alongside temp/humidity.
- **Alerts**: extend `Alerts.vue`'s severity/icon/recommendation maps for
  the two new codes, same pattern used for the pH codes.
- **Fan page** (or a new "Soil" section): expose the dry/wet calibration
  values and the watering-alert thresholds for editing, same layout as
  the existing automation settings.

## 8. Open questions before implementing

- **Option A vs B** - recommend A (555 circuit) for reliability; confirm
  before building.
- **One probe vs. one per plant** - the app tracks multiple plants per
  operation; a single soil probe only represents whichever pot it's
  physically in. Worth deciding whether one shared reading is good enough
  to start, or whether this should be a per-plant sensor from the start
  (bigger firmware/schema change - multiple ADC channels or a
  multiplexer).
- **Alert thresholds** - what % actually means "needs watering" depends
  on the specific soil mix/pot size in use, so the default 30%/80%
  bands above are placeholders to refine after a short observation
  period post-install, not a considered final value.

## 9. Status

Design only. Nothing in this document has been implemented - no firmware
module, no HTTP route, no plamp-api/frontend changes. Next step is
confirming option A vs. B and the open questions in section 8, then
building the physical probe before writing any code against it.
