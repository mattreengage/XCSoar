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

#include "ThermalSettingsPanel.hpp"
#include "Profile/ProfileKeys.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"

ThermalSettingsPanel::ThermalSettingsPanel()
  :RowFormWidget(UIGlobals::GetDialogLook()) {}

void
ThermalSettingsPanel::Prepare(ContainerWindow &parent, const PixelRect &rc)
{
  RowFormWidget::Prepare(parent, rc);

  const CirclingSettings &settings = CommonInterface::GetComputerSettings().circling;

  AddBoolean(_("Average for turn"),
             _("Should XCSoar average climb rate over 1 turn?"),
             settings.average_1_turn);

  AddInteger(_("Time to average over"),
             _("The time to average climb rate over. Also used to smooth turn average"),
             _T("%u"), _T("%u"),
             1, 30, 1, settings.average_base_time);
}

void
ThermalSettingsPanel::Show(const PixelRect &rc)
{
  CommonInterface::GetLiveBlackboard().AddListener(*this);
  RowFormWidget::Show(rc);
}

void
ThermalSettingsPanel::Hide()
{
  RowFormWidget::Hide();
  CommonInterface::GetLiveBlackboard().RemoveListener(*this);
}

bool
ThermalSettingsPanel::Save(bool &_changed)
{
  CirclingSettings &settings = CommonInterface::SetComputerSettings().circling;

  bool changed = false;

  changed |= SaveValue(Average1Turn, ProfileKeys::Average1Turn, settings.average_1_turn);
  changed |= SaveValue(AverageTimeConstant, ProfileKeys::AverageTimeConstant, settings.average_base_time);

  _changed |= changed;
  return true;
}

void
ThermalSettingsPanel::OnAction(int id) noexcept
{
}

void
ThermalSettingsPanel::OnModified(DataField &df)
{
}

