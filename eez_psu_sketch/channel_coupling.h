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
 
#pragma once

#include "persist_conf.h"
#include "ioexp.h"
#include "adc.h"
#include "dac.h"
#include "temp_sensor.h"

namespace eez {
namespace psu {
namespace channel_coupling {

enum Type {
    TYPE_NONE,
    TYPE_PARALLEL,
    TYPE_SERIES
};

void setType(Type value);
Type getType();

float getUSet(const Channel &channel);
float getUMon(const Channel &channel);
float getUMonDac(const Channel &channel);
float getULimit(const Channel &channel);
float getUMaxLimit(const Channel &channel);
float getUMin(const Channel &channel);
float getUDef(const Channel &channel);
float getUMax(const Channel &channel);
float getUProtectionLevel(const Channel &channel);
void setVoltage(Channel &channel, float voltage);
void setVoltageLimit(Channel &channel, float limit);
void setOvpParameters(Channel &channel, int state, float level, float delay);
void setOvpState(Channel &channel, int state);
void setOvpLevel(Channel &channel, float level);
void setOvpDelay(Channel &channel, float delay);

float getISet(const Channel &channel);
float getIMon(const Channel &channel);
float getIMonDac(const Channel &channel);
float getILimit(const Channel &channel);
float getIMaxLimit(const Channel &channel);
float getIMin(const Channel &channel);
float getIDef(const Channel &channel);
float getIMax(const Channel &channel);
void setCurrent(Channel &channel, float current);
void setCurrentLimit(Channel &channel, float limit);
void setOcpParameters(Channel &channel, int state, float delay);
void setOcpState(Channel &channel, int state);
void setOcpDelay(Channel &channel, float delay);

float getPowerLimit(const Channel& channel);
float getPowerMinLimit(const Channel& channel);
float getPowerMaxLimit(const Channel& channel);
float getPowerDefaultLimit(const Channel& channel);
float getPowerProtectionLevel(const Channel& channel);
void setPowerLimit(Channel &channel, float limit);

float getOppMinLevel(Channel &channel);
float getOppMaxLevel(Channel &channel);
float getOppDefaultLevel(Channel &channel);
void setOppParameters(Channel &channel, int state, float level, float delay);
void setOppState(Channel &channel, int state);
void setOppLevel(Channel &channel, float level);
void setOppDelay(Channel &channel, float delay);

void outputEnable(Channel& channel, bool enable);
bool isLowRippleAllowed(Channel& channel);
bool lowRippleEnable(Channel& channel, bool enable);
void lowRippleAutoEnable(Channel& channel, bool enable);

void clearProtection(Channel& channel);
void disableProtection(Channel& channel);

}
}
} // namespace eez::psu::channel_coupling