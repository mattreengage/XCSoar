/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "AverageVarioComputer.hpp"
#include "NMEA/MoreData.hpp"

void
AverageVarioComputer::Reset()
{
  delta_time.Reset();
  vario_30s_filter.Reset();
  netto_30s_filter.Reset();
  vario_turn_filter.Reset();
}

void
AverageVarioComputer::Compute(const MoreData &basic,
                              bool circling, bool last_circling,
                              DerivedInfo &calculated,
                              const CirclingSettings &settings)
{
  const auto dt = delta_time.Update(basic.time, 1, 0);
  if (dt < 0 || circling != last_circling) {
    Reset();
    calculated.average = basic.brutto_vario;
    calculated.netto_average = basic.netto_vario;
    calculated.turn_average = basic.brutto_vario;
    return;
  }

  if (dt <= 0)
    return;

  const unsigned Elapsed = uround(dt);
  if (Elapsed == 0)
    return;

  for (unsigned i = 0; i < Elapsed; ++i) {
    vario_30s_filter.Update(basic.brutto_vario);
    netto_30s_filter.Update(basic.netto_vario);
    vario_turn_filter.Update(basic.brutto_vario);
  }

  calculated.average = vario_30s_filter.Average();
  calculated.netto_average = netto_30s_filter.Average();

  const Angle turn_rate = calculated.turn_rate_heading_smoothed.Absolute();
  const auto circle_duration = turn_rate >= Angle::Degrees(1) ?
                  Angle::FullCircle().Native() / turn_rate.Native() :
                  settings.average_base_time;

  unsigned time = circling && settings.average_1_turn ? 
                  circle_duration : 
                  settings.average_base_time;

  calculated.turn_average = vario_turn_filter.Average((unsigned)time);
}
