/*
 * EEZ PSU Firmware
 * Copyright (C) 2016-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"
#include "temperature.h"
#include "scpi_psu.h"

#define FAN_MIN_TEMP 45 // Fan switch-on temperature (in oC)
#define FAN_MAX_TEMP 75 // Max. allowed temperature (in oC), if it stays more then FAN_MAX_TEMP_DELAY seconds then main power will be turned off.
#define FAN_MIN_PWM 12 //  PWM value for min. fan speed (12) 
#define FAN_MAX_PWM 255 // PWM value for max. fan speed (255)

#define FAN_ERR_CURRENT 1 // Max. allowed output current (in ampers) if fan or temp. sensor is invalid.
#define FAN_NOMINAL_RPM 4500 // Nominal fan RPM (for PWM=255).
#define FAN_MAX_TEMP_DELAY 30 // Number of seconds after which main power will be turned off.
#define FAN_MAX_TEMP_DROP 15 // Temperature drop (in oC) below FAN_MAX_TEMP to turn again main power on. Premature attempt to turn power on will report error -200.

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R2B6

#include "fan.h"

namespace eez {
namespace psu {
namespace fan {

TestResult test_result = psu::TEST_FAILED;

static int fan_speed_pwm = 0;
int rpm = 0;

static unsigned long test_start_time;

////////////////////////////////////////////////////////////////////////////////

int dt_to_rpm(unsigned long dt) {
	dt *= 2; // duty cycle is 50%
	dt *= 2; // 2 impulse per revolution
	return (int)(60L * 1000 * 1000 / dt);
}

unsigned long rpm_to_dt(int rpm) {
	unsigned long dt = 60L * 1000 * 1000 / rpm;
	dt /= 2;
	dt /= 2;
	return dt;
}

int pwm_to_rpm(int pwm) {
	return (int)util::remap((float)pwm, FAN_MIN_PWM, 0, FAN_MAX_PWM, FAN_NOMINAL_RPM);
}

////////////////////////////////////////////////////////////////////////////////

enum RpmMeasureState {
	RPM_MEASURE_STATE_INIT,
	RPM_MEASURE_STATE_T1,
	RPM_MEASURE_STATE_T2,
	RPM_MEASURE_STATE_FINISHED,
};

static int rpm_measure_interrupt_number;
static RpmMeasureState rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
static unsigned long rpm_measure_t1;
static unsigned long rpm_measure_t2;

void finish_rpm_measure();
void rpm_measure_interrupt_handler();

void start_rpm_measure() {
	rpm_measure_state = RPM_MEASURE_STATE_INIT;
	rpm_measure_t1 = 0;
	rpm_measure_t2 = 0;

	analogWrite(FAN_PWM, FAN_MAX_PWM);
	delay(2);
	attachInterrupt(rpm_measure_interrupt_number, rpm_measure_interrupt_handler, CHANGE);

#ifdef EEZ_PSU_SIMULATOR
	rpm_measure_t1 = 0;
	rpm_measure_t2 = rpm_to_dt(pwm_to_rpm(fan_speed_pwm));
	rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
	finish_rpm_measure();
#endif
}

void rpm_measure_interrupt_handler() {
	int x = digitalRead(FAN_SENSE);
	if (rpm_measure_state == RPM_MEASURE_STATE_INIT && x) {
		rpm_measure_state = RPM_MEASURE_STATE_T1;
	} else if (rpm_measure_state == RPM_MEASURE_STATE_T1 && !x) {
		rpm_measure_t1 = micros();
		rpm_measure_state = RPM_MEASURE_STATE_T2;
	} else if (rpm_measure_state == RPM_MEASURE_STATE_T2 && x) {
		rpm_measure_t2 = micros();
		rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
		finish_rpm_measure();
	}
}

void finish_rpm_measure() {
	detachInterrupt(rpm_measure_interrupt_number);
	analogWrite(FAN_PWM, fan_speed_pwm);

	if (rpm_measure_state == RPM_MEASURE_STATE_FINISHED) {
		rpm = dt_to_rpm(rpm_measure_t2 - rpm_measure_t1);
	} else {
		rpm_measure_state = RPM_MEASURE_STATE_FINISHED;
		rpm = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

static unsigned long fan_speed_last_measured_tick = 0;
static unsigned long fan_speed_last_adjusted_tick = 0;

bool init() {
	rpm_measure_interrupt_number = digitalPinToInterrupt(FAN_SENSE);

	return test();
}

void test_start() {
	if (OPTION_FAN) {
		analogWrite(FAN_PWM, FAN_MAX_PWM);
		test_start_time = millis();
	}
}

bool test() {
	if (OPTION_FAN) {
		unsigned long time_since_test_start = millis() - test_start_time;
		if (time_since_test_start < 250) {
			delay(300 - time_since_test_start);
		}

#ifdef EEZ_PSU_SIMULATOR
		int saved_fan_speed_pwm = fan_speed_pwm;
		fan_speed_pwm = FAN_MAX_PWM;
#endif

		start_rpm_measure();

#ifdef EEZ_PSU_SIMULATOR
		fan_speed_pwm = saved_fan_speed_pwm;
#endif

		for (int i = 0; i < 25 && rpm_measure_state != RPM_MEASURE_STATE_FINISHED; ++i) {
			delay(1);
		}

		if (rpm_measure_state != RPM_MEASURE_STATE_FINISHED) {
			finish_rpm_measure();
			test_result = psu::TEST_FAILED;
		} else {
			test_result = psu::TEST_OK;
			DebugTraceF("Fan RPM: %d", rpm);
		}
	} else {
		test_result = psu::TEST_SKIPPED;
	}

	if (test_result == psu::TEST_FAILED) {
		psu::generateError(SCPI_ERROR_FAN_TEST_FAILED);
		psu::setQuesBits(QUES_FAN, true);
	}

	return test_result != psu::TEST_FAILED;
}

void tick(unsigned long tick_usec) {
	if (test_result != psu::TEST_OK) return;

	// adjust fan speed
	if (tick_usec - fan_speed_last_adjusted_tick >= FAN_SPEED_ADJUSTMENT_INTERVAL * 1000L) {
		float maxTemperature = FAN_MIN_TEMP - 1;

		for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
			temp_sensor::TempSensor &sensor = temp_sensor::sensors[i];
			if (sensor.ch_num >= 0) {
				if (sensor.test_result == psu::TEST_OK) {
					temperature::TempSensorTemperature &sensorTemperature = temperature::sensors[i];
					if (sensorTemperature.temperature > maxTemperature) {
						maxTemperature = sensorTemperature.temperature;
					}
				} else if (sensor.test_result == psu::TEST_FAILED) {
					psu::setCurrentMaxLimit(FAN_ERR_CURRENT);
				}
			}
		}

		if (maxTemperature >= FAN_MIN_TEMP) {
			float fanSpeed = util::remap(maxTemperature, FAN_MIN_TEMP, FAN_MIN_PWM, FAN_MAX_TEMP, FAN_MAX_PWM);
			fan_speed_pwm = (int)util::clamp(fanSpeed, FAN_MIN_PWM, FAN_MAX_PWM);
			analogWrite(FAN_PWM, fan_speed_pwm);
		} else if (fan_speed_pwm != 0) {
			fan_speed_pwm = 0;
			analogWrite(FAN_PWM, fan_speed_pwm);
		}
	}

	// measure fan speed
	if (fan_speed_pwm != 0 || rpm != 0) {
		if (tick_usec - fan_speed_last_measured_tick >= TEMP_SENSOR_READ_EVERY_MS * 1000L) {
			fan_speed_last_measured_tick = tick_usec;
			start_rpm_measure();
		} else {
			if (tick_usec - fan_speed_last_measured_tick >= FAN_SPEED_MEASURMENT_INTERVAL * 1000L) {
				finish_rpm_measure();
			} else if (tick_usec - fan_speed_last_measured_tick >= 50 * 1000L) {
				if (rpm_measure_state != RPM_MEASURE_STATE_FINISHED) {
					finish_rpm_measure();

					if (fan_speed_pwm != 0) {
						test_result = psu::TEST_FAILED;
						psu::generateError(SCPI_ERROR_FAN_TEST_FAILED);
						psu::setQuesBits(QUES_FAN, true);
						psu::setCurrentMaxLimit(FAN_ERR_CURRENT);
					}
				}
			}
		}
	}
}

}
}
} // namespace eez::psu::fan

#endif