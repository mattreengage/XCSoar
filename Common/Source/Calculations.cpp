/*
  XCSoar Glide Computer
  Copyright (C) 2000 - 2004  M Roberts

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
*/

#include "stdafx.h"
#include "Calculations.h"
#include "parser.h"
#include "Utils.h"
#include "Externs.h"
#include "McReady.h"
#include "Airspace.h"
#include "Logger.h"
#include "VarioSound.h"

#include <windows.h>
#include <math.h>

#include <tchar.h>

#include "windanalyser.h"

WindAnalyser *windanalyser = NULL;

extern RECT MapRect;

static void Vario(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void LD(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void CruiseLD(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void Average30s(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void AverageThermal(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void Turning(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void LastThermalStats(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void ThermalGain(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void DistanceToNext(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void AltitudeRequired(NMEA_INFO *Basic, DERIVED_INFO *Calculated, double macready);
static void TaskStatistics(NMEA_INFO *Basic, DERIVED_INFO *Calculated, double macready);
static void InSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static int      InStartSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static int      InTurnSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void FinalGlideAlert(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void CalculateNextPosition(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void AirspaceWarning(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void AATStats(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void InAATSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static int      InAATStartSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static int      InAATurnSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void DoAutoMcReady(DERIVED_INFO *Calculated);
static void ThermalBand(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void DoLogging(NMEA_INFO *Basic, DERIVED_INFO *Calculated);


static void TerrainHeight(NMEA_INFO *Basic, DERIVED_INFO *Calculated);
static void SortLandableWaypoints(NMEA_INFO *Basic, DERIVED_INFO *Calculated);



int getFinalWaypoint() {
  int i;
  i=ActiveWayPoint;

  i++;
  while((Task[i].Index != -1) && (i<MAXTASKPOINTS))
    {
      i++;
    }
  return i-1;
}

int FastLogNum = 0; // number of points to log at high rate


void AddSnailPoint(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{

  SnailTrail[SnailNext].Lattitude = Basic->Lattitude;
  SnailTrail[SnailNext].Longditude = Basic->Longditude;

  // JMW TODO: if circling, color according to 30s average?
  SnailTrail[SnailNext].Vario = Calculated->Vario ;

  SnailNext ++;
  SnailNext %= TRAILSIZE;

}


void DoLogging(NMEA_INFO *Basic, DERIVED_INFO *Calculated) {
  static double SnailLastTime=0;
  static double LogLastTime=0;
  double dtLog = 5.0;
  double dtSnail = 2.0;

  if(Basic->Time <= LogLastTime)
    {
      LogLastTime = Basic->Time;
    }
  if(Basic->Time <= SnailLastTime)
    {
      SnailLastTime = Basic->Time;
    }

  if (FastLogNum) {
    dtLog = 1.0;
    dtSnail = 1.0;
  }

  if (Basic->Time - LogLastTime >= dtLog) {
    if(LoggerActive) {
      LogPoint(Basic->Lattitude , Basic->Longditude , Basic->Altitude );
    }
    LogLastTime += dtLog;

    if (FastLogNum) FastLogNum--;

  }

  if (Basic->Time - SnailLastTime >= dtSnail) {
    AddSnailPoint(Basic, Calculated);
    SnailLastTime += dtSnail;
  }
}


void AudioVario(NMEA_INFO *Basic, DERIVED_INFO *Calculated) {
  if (
      (Basic->AirspeedAvailable &&
      (Basic->Airspeed >= NettoSpeed))
      ||
      (!Basic->AirspeedAvailable &&
       (Basic->Speed >= NettoSpeed))
      ) {
    // TODO: slow/smooth switching between netto and not

    double theSinkRate;
    double n;
    if (Basic->AccelerationAvailable) {
      n = Basic->Gload;
    } else {
      n = 1.0;
    }

    if (Basic->AirspeedAvailable) {
      theSinkRate= SinkRate(Basic->Airspeed, n);
    } else {
      // assume zero wind (Speed=Airspeed, very bad I know)
      theSinkRate= SinkRate(Basic->Speed, n);
    }

    if (Basic->VarioAvailable) {
      Calculated->NettoVario = Basic->Vario - theSinkRate;
    } else {
      Calculated->NettoVario = Calculated->Vario - theSinkRate;
    }

    VarioSound_SetV((short)(Calculated->NettoVario/6.0*100));

  } else {
    if (Basic->VarioAvailable) {
      VarioSound_SetV((short)(Basic->Vario/6.0*100));
    } else {
      VarioSound_SetV((short)(Calculated->Vario/6.0*100));
    }
  }
}


BOOL DoCalculationsVario(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTime = 0;

  AudioVario(Basic, Calculated);

  // has GPS time advanced?
  if(Basic->Time <= LastTime)
    {
      LastTime = Basic->Time;
      return FALSE;
    }

  LastTime = Basic->Time;

  return TRUE;
}


BOOL DoCalculations(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTime = 0;
  static double macready;

  if (!windanalyser) {
    windanalyser = new WindAnalyser(Basic, Calculated);

    // seed initial wind store with current conditions
    Vector v;
    v.x = Calculated->WindSpeed*cos(Calculated->WindBearing*3.1415926/180.0);
    v.y = Calculated->WindSpeed*sin(Calculated->WindBearing*3.1415926/180.0);

    windanalyser->slot_newEstimate(v, 3);

  }

  macready = MACREADY/LIFTMODIFY;

  DistanceToNext(Basic, Calculated);
  AltitudeRequired(Basic, Calculated, macready);

  TerrainHeight(Basic, Calculated);

  if (TaskAborted) {
    SortLandableWaypoints(Basic, Calculated);
  }
  TaskStatistics(Basic, Calculated, macready);

  if(Basic->Time <= LastTime)
    {
      LastTime = Basic->Time;
      return FALSE;
    }

  LastTime = Basic->Time;

  if ((Calculated->FinalGlide)
      ||(fabs(Calculated->TaskAltitudeDifference)>30)) {
    FinalGlideAlert(Basic, Calculated);
    if (Calculated->AutoMcReady) {
      DoAutoMcReady(Calculated);
    }
  }

  Turning(Basic, Calculated);
  Vario(Basic,Calculated);
  LD(Basic,Calculated);
  CruiseLD(Basic,Calculated);
  Average30s(Basic,Calculated);
  AverageThermal(Basic,Calculated);
  ThermalGain(Basic,Calculated);
  LastThermalStats(Basic, Calculated);
  ThermalBand(Basic, Calculated);

  DistanceToNext(Basic, Calculated);

  // do we need to do this twice?
  if (TaskAborted) {

    SortLandableWaypoints(Basic, Calculated);
    //    TaskStatistics(Basic, Calculated, macready);

  } else {

    InSector(Basic, Calculated);
    InAATSector(Basic, Calculated);

    AATStats(Basic, Calculated);
    TaskStatistics(Basic, Calculated, macready);

  }

  AltitudeRequired(Basic, Calculated, macready);
  TerrainHeight(Basic, Calculated);

  CalculateNextPosition(Basic, Calculated);

  AirspaceWarning(Basic, Calculated);

  DoLogging(Basic, Calculated);

  return TRUE;
}

void Vario(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTime = 0;
  static double LastAlt = 0;
  double Gain;

  if(Basic->Time > LastTime)
    {
      Gain = Basic->Altitude - LastAlt;

      if (!Basic->VarioAvailable) {
        // estimate value from GPS
        Calculated->Vario = Gain / (Basic->Time - LastTime);
      } else {
        // get value from instrument
        Calculated->Vario = Basic->Vario;
        // we don't bother with sound here as it is polled at a
        // faster rate in the DoVarioCalcs methods
      }

      LastAlt = Basic->Altitude;
      LastTime = Basic->Time;

    }
  else
    {
      LastTime = Basic->Time;
    }
}


void Average30s(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTime = 0;
  static double Altitude[30];
  int Elapsed, i;
  long temp;
  double Gain;


  if(Basic->Time > LastTime)
    {
      Elapsed = (int)(Basic->Time - LastTime);
      for(i=0;i<Elapsed;i++)
        {
          temp = (long)LastTime + i;
          temp %=30;

          Altitude[temp] = Basic->Altitude;
        }
      temp = (long)Basic->Time - 1;
      temp = temp%30;
      Gain = Altitude[temp];

      temp = (long)Basic->Time;
      temp = temp%30;
      Gain = Gain - Altitude[temp];

      LastTime = Basic->Time;
      Calculated->Average30s = Gain/30;
    }
  else
    {
      LastTime = Basic->Time;
    }


}

void AverageThermal(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  double Gain;

  if(Basic->Time > Calculated->ClimbStartTime)
    {
      Gain = Basic->Altitude - Calculated->ClimbStartAlt;
      Calculated->AverageThermal  = Gain / (Basic->Time - Calculated->ClimbStartTime);
    }
}

void ThermalGain(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  if(Basic->Time > Calculated->ClimbStartTime)
    {
      Calculated->ThermalGain = Basic->Altitude - Calculated->ClimbStartAlt;
    }
}

void LD(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastLat = 0;
  static double LastLon = 0;
  static double LastTime = 0;
  static double LastAlt = 0;
  double DistanceFlown;
  double AltLost;


  if(Basic->Time - LastTime >20)
    {
      DistanceFlown = Distance(Basic->Lattitude, Basic->Longditude, LastLat, LastLon);
      AltLost = LastAlt - Basic->Altitude;
      if(AltLost > 0)
        {
          Calculated->LD = DistanceFlown / AltLost;
          if(Calculated->LD>999)
            {
              Calculated->LD = 999;
            }
        }
      else if (AltLost < 0) {
        // JMW added negative LD calculations TODO: TEST

        Calculated->LD = DistanceFlown / AltLost;
        if (Calculated->LD<-999) {
          Calculated->LD = -999;
        }
      } else {
        Calculated->LD = 999;
      }

      LastLat = Basic->Lattitude;
      LastLon = Basic->Longditude;
      LastAlt = Basic->Altitude;
      LastTime = Basic->Time;
    }
}

void CruiseLD(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastLat = 0;
  static double LastLon = 0;
  static double LastTime = 0;
  static double LastAlt = 0;
  double DistanceFlown;
  double AltLost;


  if(!Calculated->Circling)
    {

      DistanceFlown = Distance(Basic->Lattitude, Basic->Longditude, Calculated->CruiseStartLat, Calculated->CruiseStartLong);
      AltLost = Calculated->CruiseStartAlt - Basic->Altitude;
      if(AltLost > 0)
        {
          Calculated->CruiseLD = DistanceFlown / AltLost;
          if(Calculated->CruiseLD>999)
            {
              Calculated->CruiseLD = 999;
            }
        }
      // JMW added negative LD calculations TODO: TEST
      else if (AltLost <0) {
        Calculated->CruiseLD = DistanceFlown / AltLost;
        if(Calculated->CruiseLD< -999)
          {
            Calculated->CruiseLD = -999;
          }
      } else {
        Calculated->CruiseLD = 999;
      }
    }
}

#define CRUISE 0
#define WAITCLIMB 1
#define CLIMB 2
#define WAITCRUISE 3


double MinTurnRate = 4 ; //10;
double CruiseClimbSwitch = 15;
double ClimbCruiseSwitch = 15;


void SwitchZoomClimb(bool isclimb, bool left) {

  static double CruiseMapScale = 10;
  static double ClimbMapScale = 0.25;
  // if AutoZoom

  // JMW
  if (EnableSoundTask) {
    PlayResource(TEXT("IDR_WAV_DRIP"));
  }

  if (CircleZoom) {
    if (isclimb) {
      CruiseMapScale = RequestMapScale;
      RequestMapScale = ClimbMapScale;
    } else {
      // leaving climb
      ClimbMapScale = RequestMapScale;
      RequestMapScale = CruiseMapScale;
    }
  }

  windanalyser->slot_newFlightMode(left, 0);

}


void Turning(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTrack = 0;
  static double StartTime  = 0;
  static double StartLong = 0;
  static double StartLat = 0;
  static double StartAlt = 0;
  static double LastTime = 0;
  static double timeCircling = 0;
  static double timeCruising = 0;
  static int MODE = CRUISE;
  static bool LEFT = FALSE;
  double Rate;

  if(Basic->Time <= LastTime)
    return;

  if((LastTrack>270) && (Basic->TrackBearing <90))
    {
      Rate = Basic->TrackBearing + (360-LastTrack);
    }
  else if ((LastTrack<90) && (Basic->TrackBearing >270))
    {
      Rate = LastTrack + (360-Basic->TrackBearing );
    }
  else
    {
      Rate = (Basic->TrackBearing - LastTrack);
    }
  Rate = Rate / (Basic->Time - LastTime);

  // JMW added percent climb calculator

  if (Calculated->Circling) {
    //    timeCircling += (Basic->Time-LastTime);
    timeCircling+= 1.0;
  } else {
    //    timeCruising += (Basic->Time-LastTime);
    timeCruising+= 1.0;
  }

  if (timeCruising+timeCircling>0) {
    Calculated->PercentCircling = 100.0*(timeCircling)/(timeCruising+timeCircling);
  } else {
    Calculated->PercentCircling = 0.0;
  }

  if(Rate <0)
    {
      if (LEFT) {
        // OK, already going left
      } else {
        LEFT = true;
      }
      Rate *= -1;
    } else {
    if (!LEFT) {
      // OK, already going right
    } else {
      LEFT = false;
    }
  }
  // JMW TODO: switch flight modes if changing direction?

  LastTime = Basic->Time;
  LastTrack = Basic->TrackBearing;

  if(MODE == CRUISE)
    {
      if(Rate > MinTurnRate)
        {
          StartTime = Basic->Time;
          StartLong = Basic->Longditude;
          StartLat  = Basic->Lattitude;
          StartAlt  = Basic->Altitude;
          MODE = WAITCLIMB;
        }
    }
  else if(MODE == WAITCLIMB)
    {
      if(Rate > MinTurnRate)
        {
          if( (Basic->Time  - StartTime) > CruiseClimbSwitch)
            {
              Calculated->Circling = TRUE;
              // JMW Transition to climb
              MODE = CLIMB;
              Calculated->ClimbStartLat = StartLat;
              Calculated->ClimbStartLong = StartLong;
              Calculated->ClimbStartAlt = StartAlt;
              Calculated->ClimbStartTime = StartTime;

              SwitchZoomClimb(true, LEFT);

            }
        }
      else
        {
          MODE = CRUISE;
        }
    }
  else if(MODE == CLIMB)
    {
      windanalyser->slot_newSample();

      if(Rate < MinTurnRate)
        {
          StartTime = Basic->Time;
          StartLong = Basic->Longditude;
          StartLat  = Basic->Lattitude;
          StartAlt  = Basic->Altitude;
          // JMW Transition to cruise
          MODE = WAITCRUISE;
        }
    }
  else if(MODE == WAITCRUISE)
    {
      if(Rate < MinTurnRate)
        {
          if( (Basic->Time  - StartTime) > ClimbCruiseSwitch)
            {
              Calculated->Circling = FALSE;

              // Transition to cruise
              MODE = CRUISE;
              Calculated->CruiseStartLat = StartLat;
              Calculated->CruiseStartLong = StartLong;
              Calculated->CruiseStartAlt = StartAlt;
              Calculated->CruiseStartTime = StartTime;

              SwitchZoomClimb(false, LEFT);

            }
        }
      else
        {
          // JMW Transition to climb
          MODE = CLIMB;
        }
    }

  windanalyser->slot_Altitude();

}

static void LastThermalStats(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static int LastCircling = FALSE;
  double ThermalGain;
  double ThermalTime;
  double ThermalDrift;
  double DriftAngle;

  if((Calculated->Circling == FALSE) && (LastCircling == TRUE))
    {
      ThermalGain = Calculated->CruiseStartAlt - Calculated->ClimbStartAlt;
      ThermalTime = Calculated->CruiseStartTime - Calculated->ClimbStartTime;

      ThermalDrift = Distance(Calculated->CruiseStartLat,  Calculated->CruiseStartLong, Calculated->ClimbStartLat,  Calculated->ClimbStartLong);
      DriftAngle = Bearing(Calculated->ClimbStartLat,  Calculated->ClimbStartLong,Calculated->CruiseStartLat, Calculated->CruiseStartLong);

      if(ThermalTime >0)
        {
          Calculated->LastThermalAverage = ThermalGain/ThermalTime;
          Calculated->LastThermalGain = ThermalGain;
          Calculated->LastThermalTime = ThermalTime;
          if(ThermalTime > 120)
            {

              /* Don't set it immediately, go through the new
                 wind model
              Calculated->WindSpeed = ThermalDrift/ThermalTime;

              if(DriftAngle >=180)
                DriftAngle -= 180;
              else
                DriftAngle += 180;

              Calculated->WindBearing = DriftAngle;
              */

	      Vector v;
	      v.x = ThermalDrift/ThermalTime*cos(DriftAngle*3.1415926/180.0);
	      v.y = ThermalDrift/ThermalTime*sin(DriftAngle*3.1415926/180.0);

              windanalyser->slot_newEstimate(v, 6);
              // 6 is the code for external estimates

            }
        }
    }
  LastCircling = Calculated->Circling;
}

void DistanceToNext(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  if(ActiveWayPoint >=0)
    {
      Basic->WaypointDistance = Distance(Basic->Lattitude, Basic->Longditude,
                                         WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                                         WayPointList[Task[ActiveWayPoint].Index].Longditude);
      Basic->WaypointBearing = Bearing(Basic->Lattitude, Basic->Longditude,
                                       WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                                       WayPointList[Task[ActiveWayPoint].Index].Longditude);
    }
  else
    {
      Basic->WaypointDistance = 0;
    }
}

void AltitudeRequired(NMEA_INFO *Basic, DERIVED_INFO *Calculated, double macready)
{
  if(ActiveWayPoint >=0)
    {
      Calculated->NextAltitudeRequired =
        McReadyAltitude(macready,
                        Basic->WaypointDistance,Basic->WaypointBearing,
                        Calculated->WindSpeed, Calculated->WindBearing,
                        0, 0, (ActiveWayPoint == getFinalWaypoint())
                        // ||
                        // (Calculated->TaskAltitudeDifference>30)
                        // JMW TODO!!!!!!!!!
                        );
      Calculated->NextAltitudeRequired =
        Calculated->NextAltitudeRequired * (1/BUGS);

      Calculated->NextAltitudeRequired =
        Calculated->NextAltitudeRequired + SAFETYALTITUDEARRIVAL ;

      Calculated->NextAltitudeDifference =
        Basic->Altitude - (Calculated->NextAltitudeRequired
                           + WayPointList[Task[ActiveWayPoint].Index].Altitude);
    }
  else
    {
      Calculated->NextAltitudeRequired = 0;
      Calculated->NextAltitudeDifference = 0;
    }
}

int InTurnSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  double AircraftBearing;

  if(FAISector !=  TRUE)
    {
      if(Basic->WaypointDistance < SectorRadius)
        {
          return TRUE;
        }
    }
  // else
  {
    AircraftBearing = Bearing(WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                              WayPointList[Task[ActiveWayPoint].Index].Longditude,
                              Basic->Lattitude ,
                              Basic->Longditude);

    AircraftBearing = AircraftBearing - Task[ActiveWayPoint].Bisector ;

    if( (AircraftBearing >= -45) && (AircraftBearing <= 45))
      {
        if(Basic->WaypointDistance < 20000)
          {
            return TRUE;
          }
      }
  }
  return FALSE;
}

int InAATTurnSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  double AircraftBearing;

  if(Task[ActiveWayPoint].AATType ==  CIRCLE)
    {
      if(Basic->WaypointDistance < Task[ActiveWayPoint].AATCircleRadius)
        {
          return TRUE;
        }
    }
  else if(Basic->WaypointDistance < Task[ActiveWayPoint].AATSectorRadius)
    {

      AircraftBearing = Bearing(WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                                WayPointList[Task[ActiveWayPoint].Index].Longditude,
                                Basic->Lattitude ,
                                Basic->Longditude);

      if(Task[ActiveWayPoint].AATStartRadial < Task[ActiveWayPoint].AATFinishRadial )
        {
          if(
             (AircraftBearing > Task[ActiveWayPoint].AATStartRadial)
             &&
             (AircraftBearing < Task[ActiveWayPoint].AATFinishRadial)
             )
            return TRUE;
        }

      if(Task[ActiveWayPoint].AATStartRadial > Task[ActiveWayPoint].AATFinishRadial )
        {
          if(
             (AircraftBearing > Task[ActiveWayPoint].AATStartRadial)
             ||
             (AircraftBearing < Task[ActiveWayPoint].AATFinishRadial)
             )
            return TRUE;
        }
    }
  return FALSE;
}


int InStartSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static int InSector = FALSE;
  double AircraftBearing;
  double FirstPointDistance;

  // No Task Loaded
  if(Task[0].Index == -1)
    {
      return FALSE;
    }


  FirstPointDistance = Distance(Basic->Lattitude ,Basic->Longditude ,WayPointList[Task[0].Index].Lattitude , WayPointList[Task[0].Index].Longditude);

  if(!StartLine) // Start Circle
    {
      if(FirstPointDistance< StartRadius)
        {
          return TRUE;
        }
      else
        {
          return FALSE;
        }
    }

  // Start Line
  AircraftBearing = Bearing(WayPointList[Task[0].Index].Lattitude,
                            WayPointList[Task[0].Index].Longditude,
                            Basic->Lattitude ,
                            Basic->Longditude);

  AircraftBearing = AircraftBearing - Task[0].Bisector ;

  if( (AircraftBearing >= -90) && (AircraftBearing <= 90))
    {
      if(FirstPointDistance < StartRadius)
        {
          return TRUE;
        }
    }
  return FALSE;
}

void AnnounceWayPointSwitch() {

  // start logging data at faster rate
  FastLogNum = 5;

  // play sound
  if (EnableSoundTask) {
    PlayResource(TEXT("IDR_WAV_TASKTURNPOINT"));
  }

}


void InSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static BOOL StartSectorEntered = FALSE;

  if(AATEnabled)
    return;

  if(ActiveWayPoint == 0)
    {
      if(InStartSector(Basic,Calculated))
        {
          StartSectorEntered = TRUE;
        }
      else
        {
          if(StartSectorEntered == TRUE)
            {
              if(ActiveWayPoint < MAXTASKPOINTS)
                {
                  if(Task[ActiveWayPoint+1].Index >= 0)
                    {
                      ActiveWayPoint ++;
                      Calculated->TaskStartTime = Basic->Time ;
                      Calculated->LegStartTime = Basic->Time;
                      StartSectorEntered = FALSE;

                      AnnounceWayPointSwitch();
                    }
                }
            }
        }
    }
  else if(ActiveWayPoint >0)
    {
      // JMW what does this do? restart?
      if(InStartSector(Basic, Calculated))
        {
          if(Basic->Time - Calculated->TaskStartTime < 600)
            {
              ActiveWayPoint = 0;
              StartSectorEntered = TRUE;
            }
        }

      if(InTurnSector(Basic,Calculated))
        {
          if(ActiveWayPoint < MAXTASKPOINTS)
            {
              if(Task[ActiveWayPoint+1].Index >= 0)
                {
                  Calculated->LegStartTime = Basic->Time;

                  ActiveWayPoint ++;
                  AnnounceWayPointSwitch();

                  return;
                }
            }
        }
    }
}

void InAATSector(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static BOOL StartSectorEntered = FALSE;

  if(!AATEnabled)
    return;

  if(ActiveWayPoint == 0)
    {
      if(InStartSector(Basic,Calculated))
        {
          StartSectorEntered = TRUE;
        }
      else
        {
          if(StartSectorEntered == TRUE)
            {
              if(ActiveWayPoint < MAXTASKPOINTS)
                {
                  if(Task[ActiveWayPoint+1].Index >= 0)
                    {

                      ActiveWayPoint ++;
                      Calculated->TaskStartTime = Basic->Time ;
                      Calculated->LegStartTime = Basic->Time;
                      StartSectorEntered = FALSE;

                      AnnounceWayPointSwitch();

                    }
                }
            }
        }
    }
  else if(ActiveWayPoint >0)
    {
      if(InStartSector(Basic, Calculated))
        {
          if(Basic->Time - Calculated->TaskStartTime < 600)
            {
              ActiveWayPoint = 0;
              StartSectorEntered = TRUE;
            }
        }
      if(InAATTurnSector(Basic,Calculated))
        {
          if(ActiveWayPoint < MAXTASKPOINTS)
            {
              if(Task[ActiveWayPoint+1].Index >= 0)
                {
                  Calculated->LegStartTime = Basic->Time;

                  AnnounceWayPointSwitch();
                  ActiveWayPoint ++;

                  return;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////





///////////////////////////////////
#include "RasterTerrain.h"

RasterTerrain terrain_dem;

void OpenTerrain(void) {
  LockTerrainData();
  terrain_dem.OpenTerrain();
  UnlockTerrainData();
}

void CloseTerrain(void) {
  LockTerrainData();
  terrain_dem.CloseTerrain();
  UnlockTerrainData();
}


static void TerrainHeight(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  double Alt = 0;

  LockTerrainData();
  terrain_dem.SetTerrainRounding(0);
  Alt = terrain_dem.GetTerrainHeight(Basic->Lattitude , Basic->Longditude);
  UnlockTerrainData();

  if(Alt<0) Alt = 0;

  Calculated->TerrainAlt = Alt;
  Calculated->AltitudeAGL = Basic->Altitude - Calculated->TerrainAlt;
}


/////////////////////////////////////////

void TaskStatistics(NMEA_INFO *Basic, DERIVED_INFO *Calculated, double macready)
{
  int i;
  double LegCovered, LegToGo, LegDistance, LegBearing, LegAltitude;
  double TaskAltitudeRequired = 0;

  // Calculate Task Distances
  if(ActiveWayPoint >=1)
    {
      LegDistance =
        Distance(WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                 WayPointList[Task[ActiveWayPoint].Index].Longditude,
                 WayPointList[Task[ActiveWayPoint-1].Index].Lattitude,
                 WayPointList[Task[ActiveWayPoint-1].Index].Longditude);

      LegToGo =
        Distance(Basic->Lattitude , Basic->Longditude ,
                 WayPointList[Task[ActiveWayPoint].Index].Lattitude,
                 WayPointList[Task[ActiveWayPoint].Index].Longditude);

      LegCovered = LegDistance - LegToGo;

      if(LegCovered <=0)
        Calculated->TaskDistanceCovered = 0;
      else
        Calculated->TaskDistanceCovered = LegCovered;

      Calculated->LegDistanceToGo = LegToGo;
      Calculated->LegDistanceCovered = Calculated->TaskDistanceCovered;

      if(Basic->Time != Calculated->LegStartTime)
        Calculated->LegSpeed = Calculated->LegDistanceCovered
          / (Basic->Time - Calculated->LegStartTime);


      for(i=0;i<ActiveWayPoint-1;i++)
        {
          LegDistance =
            Distance(WayPointList[Task[i].Index].Lattitude,
                     WayPointList[Task[i].Index].Longditude,
                     WayPointList[Task[i+1].Index].Lattitude,
                     WayPointList[Task[i+1].Index].Longditude);

          Calculated->TaskDistanceCovered += LegDistance;

        }

      if(Basic->Time != Calculated->TaskStartTime)
        Calculated->TaskSpeed =
          Calculated->TaskDistanceCovered
          / (Basic->Time - Calculated->TaskStartTime);
    }

  // Calculate Final Glide To Finish
  Calculated->TaskDistanceToGo = 0;
  if(ActiveWayPoint >=0)
    {
      i=ActiveWayPoint;

      int FinalWayPoint = getFinalWaypoint();

      LegBearing = Bearing(Basic->Lattitude , Basic->Longditude ,
                           WayPointList[Task[i].Index].Lattitude,
                           WayPointList[Task[i].Index].Longditude);

      LegToGo = Distance(Basic->Lattitude , Basic->Longditude ,
                         WayPointList[Task[i].Index].Lattitude,
                         WayPointList[Task[i].Index].Longditude);

      // JMW TODO: use instantaneous macready here again to calculate
      // dolphin speed to fly
      LegAltitude = McReadyAltitude(macready, LegToGo, LegBearing,
                                    Calculated->WindSpeed,
                                    Calculated->WindBearing,
                                    &(Calculated->BestCruiseTrack),
                                    &(Calculated->VMcReady),
                                    (i==FinalWayPoint)
                                    // ||()
                                    // JMW TODO!!!!!!!!!!!
                                    );

      if ((i==FinalWayPoint)||(TaskAborted)) {
        double lat, lon;
        double distancesoarable =
          FinalGlideThroughTerrain(LegBearing, Basic, Calculated,
                                   &lat,
                                   &lon);

        if (distancesoarable< LegToGo) {
          // JMW TODO display terrain warning
          Calculated->TerrainWarningLattitude = lat;
          Calculated->TerrainWarningLongditude = lon;

        } else {
          Calculated->TerrainWarningLattitude = 0.0;
          Calculated->TerrainWarningLongditude = 0.0;

        }

      } else {
        Calculated->TerrainWarningLattitude = 0.0;
        Calculated->TerrainWarningLongditude = 0.0;
      }

      LegAltitude = LegAltitude * (1/BUGS);

      TaskAltitudeRequired = LegAltitude;
      Calculated->TaskDistanceToGo = LegToGo;

      i++;
      while((Task[i].Index != -1) && (i<MAXTASKPOINTS) && (!TaskAborted))
        {
          LegDistance = Distance(WayPointList[Task[i].Index].Lattitude,
                                 WayPointList[Task[i].Index].Longditude,
                                 WayPointList[Task[i-1].Index].Lattitude,
                                 WayPointList[Task[i-1].Index].Longditude);

          LegBearing = Bearing(WayPointList[Task[i-1].Index].Lattitude,
                               WayPointList[Task[i-1].Index].Longditude,
                               WayPointList[Task[i].Index].Lattitude,
                               WayPointList[Task[i].Index].Longditude);


          LegAltitude = McReadyAltitude(macready, LegDistance, LegBearing,
                                        Calculated->WindSpeed, Calculated->WindBearing, 0, 0,
                                        (i==FinalWayPoint) // ||() JMW TODO!!!!!!!!!
                                        );
          LegAltitude = LegAltitude * (1/BUGS);

          TaskAltitudeRequired += LegAltitude;

          Calculated->TaskDistanceToGo += LegDistance;

          i++;
        }

      if ((ActiveWayPoint == FinalWayPoint)||(TaskAborted)) {
        // JMW on final glide
        Calculated->FinalGlide = 1;
      } else {
        Calculated->FinalGlide = 0;
      }


      Calculated->TaskAltitudeRequired = TaskAltitudeRequired + SAFETYALTITUDEARRIVAL;
      Calculated->TaskAltitudeDifference = Basic->Altitude - (Calculated->TaskAltitudeRequired + WayPointList[Task[i-1].Index].Altitude);

      if(  (Basic->Altitude - WayPointList[Task[i-1].Index].Altitude) > 0)
        {
          Calculated->LDFinish = Calculated->TaskDistanceToGo / (Basic->Altitude - WayPointList[Task[i-1].Index].Altitude)  ;
        }
      else
        {
          Calculated->LDFinish = 9999;
        }

    } else {
    // no task selected, so work things out at current heading

    McReadyAltitude(macready, 100.0, Basic->TrackBearing,
                    Calculated->WindSpeed,
                    Calculated->WindBearing,
                    &(Calculated->BestCruiseTrack),
                    &(Calculated->VMcReady),
                    0
                    // ||()
                    // JMW TODO!!!!!!!!!!!
                    );

  }
}

void DoAutoMcReady(DERIVED_INFO *Calculated)
{
  static double tad=0.0;
  static double dmc=0.0;

  tad = Calculated->TaskAltitudeDifference;

  if (tad > 20) {
    dmc = dmc*0.2+0.8*0.2;
  } else if (tad < -20) {
    dmc = dmc*0.2-0.8*0.2;
  }
  MACREADY += dmc;
  if (MACREADY>20.0) {
    MACREADY = 20.0;
  }
  if (MACREADY<0.0) {
    MACREADY = 0.0;
  }

  /* NOT WORKING
  static double tad=0.0;
  static double mclast = 0.0;
  static double tadlast= 0.0;
  static double slope = 0.0;
  double mcnew;
  double delta;

  tad = Calculated->TaskAltitudeDifference;

  if (fabs(tad)<5.0) {
    tadlast = tad;
    mclast = MACREADY;
    return;
  }

  // no change detected, increment until see something

  if (fabs(tad-tadlast)>0.0001) {
    slope = 0.9*slope+0.1*(MACREADY-mclast)/(tad-tadlast);
  } else {
  }

  if (fabs(slope)<0.01) {
    if (tad>0) {
      mcnew= MACREADY+0.1;
    } else {
      mcnew= MACREADY-0.1;
    }
  } else {

    // y = mx + c
    // -c = mx
    // x = -c/m
    // 5 -> 100
    // 4 -> 200
    // slope=(5-4)/(100-200)= -0.1
    delta = (-slope*tad);
    delta = min(1.0,max(-1.0,delta));
    mcnew = MACREADY+0.3*(delta);
  }
  tadlast = tad;
  mclast = MACREADY;

  MACREADY = mcnew;
  if (MACREADY>10.0) {
    MACREADY = 10.0;
  }
  if (MACREADY<0.0) {
    MACREADY = 0.0;
  }
  */
}

void FinalGlideAlert(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static BOOL BelowGlide = TRUE;

  if(BelowGlide == TRUE)
    {
      if(Calculated->TaskAltitudeDifference > 10)
        {
          BelowGlide = FALSE;

          sndPlaySound(TEXT("My Documents\\FinalGlide.wav"),SND_ASYNC|SND_NODEFAULT);
        }
    }
  else
    {
      if(Calculated->TaskAltitudeDifference < 10)
        {
          BelowGlide = TRUE;
          sndPlaySound(TEXT("My Documents\\Tiptoe.wav"),SND_ASYNC|SND_NODEFAULT);
        }
    }
}

extern int AIRSPACEWARNINGS;
extern int WarningTime;
extern int AcknowledgementTime;


void CalculateNextPosition(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  if(Calculated->Circling)
    {
      Calculated->NextLattitude = Basic->Lattitude;
      Calculated->NextLongditde = Basic->Longditude;
      Calculated->NextAltitude = Basic->Altitude + Calculated->Average30s * 30;
    }
  else
    {
      Calculated->NextLattitude = FindLattitude(Basic->Lattitude, Basic->Longditude, Basic->TrackBearing, Basic->Speed*WarningTime );
      Calculated->NextLongditde = FindLongditude(Basic->Lattitude, Basic->Longditude, Basic->TrackBearing, Basic->Speed*WarningTime);
      Calculated->NextAltitude = Basic->Altitude + Calculated->Average30s * WarningTime;
    }
}

int LastCi =-1;
int LastAi =-1;
HWND hCMessage = NULL;
HWND hAMessage = NULL;
int ClearAirspaceWarningTimeout = 0;

bool ClearAirspaceWarnings() {
  if(hCMessage)
    {
      LastCi = -1;LastAi = -1;
      DestroyWindow(hCMessage);
      ClearAirspaceWarningTimeout = AcknowledgementTime;
      hCMessage = NULL;
      return true;
    }
  if(hAMessage)
    {
      ClearAirspaceWarningTimeout = AcknowledgementTime;
      DestroyWindow(hAMessage);
      hAMessage = NULL;
      return true;
    }
  return false;
}

void AirspaceWarning(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  int i;
  TCHAR szMessageBuffer[1024];
  TCHAR szTitleBuffer[1024];

  if (ClearAirspaceWarningTimeout) {
    ClearAirspaceWarningTimeout--;
  }

  if(!AIRSPACEWARNINGS)
    {
      if(hCMessage)
        {
          LastCi = -1;LastAi = -1;
          DestroyWindow(hCMessage);
          hCMessage = NULL;
        }
      if(hAMessage)
        {
          DestroyWindow(hAMessage);
          hAMessage = NULL;
        }
      return;
    }

  i= FindAirspaceCircle(Calculated->NextLongditde, Calculated->NextLattitude );
  if(i != -1)
    {
      if(i == LastCi)
        {   // already being displayed
          return;
        }

      if(hCMessage)
        {
          DestroyWindow(hCMessage);
          hCMessage = NULL;
        }

      if (ClearAirspaceWarningTimeout>0) {
	return;
      }

      MessageBeep(MB_ICONEXCLAMATION);
      FormatWarningString(AirspaceCircle[i].Type , AirspaceCircle[i].Name , AirspaceCircle[i].Base, AirspaceCircle[i].Top, szMessageBuffer, szTitleBuffer );
      if(
         (DisplayOrientation == TRACKUP)
         ||
         ((DisplayOrientation == NORTHCIRCLE) && (Calculated->Circling == FALSE))
	  ||
         ((DisplayOrientation == TRACKCIRCLE) && (Calculated->Circling == FALSE) )
         )
        hCMessage = CreateWindow(TEXT("EDIT"),szMessageBuffer,WS_VISIBLE|WS_CHILD|ES_MULTILINE |ES_CENTER|WS_BORDER|ES_READONLY,
                                 0, MapRect.top+15, 240, 50, hWndMapWindow,NULL,hInst,NULL);
      else
        hCMessage = CreateWindow(TEXT("EDIT"),szMessageBuffer,WS_VISIBLE|WS_CHILD|ES_MULTILINE |ES_CENTER|WS_BORDER|ES_READONLY,
                                 0, 180,240,50,hWndMapWindow,NULL,hInst,NULL);

      ShowWindow(hCMessage,SW_SHOW);
      UpdateWindow(hCMessage);
      LastCi = i;
      return;
    }
  else if(hCMessage)
    {
      DestroyWindow(hCMessage);
      hCMessage = NULL;
      LastCi = -1;
    }
  else
    {
      LastCi = -1;
    }


  i= FindAirspaceArea(Calculated->NextLongditde,Calculated->NextLattitude);
  if(i != -1)
    {
      if(i == LastAi)
        {
          return;
        }

      if(hAMessage)
        {
          DestroyWindow(hAMessage);
          hAMessage = NULL;
        }

      if (ClearAirspaceWarningTimeout>0) {
	return;
      }

      MessageBeep(MB_ICONEXCLAMATION);
      FormatWarningString(AirspaceArea[i].Type , AirspaceArea[i].Name , AirspaceArea[i].Base, AirspaceArea[i].Top, szMessageBuffer, szTitleBuffer );
      if(
         (DisplayOrientation == TRACKUP)
         ||
         ((DisplayOrientation == NORTHCIRCLE) && (Calculated->Circling == FALSE) )  	  ||
         ((DisplayOrientation == TRACKCIRCLE) && (Calculated->Circling == FALSE) )
         )

        hAMessage = CreateWindow(TEXT("EDIT"),szMessageBuffer,WS_VISIBLE|WS_CHILD|ES_MULTILINE |ES_CENTER|WS_BORDER|ES_READONLY,
                                 0, 15+MapRect.top, 240,50,hWndMapWindow,NULL,hInst,NULL);
      else
        hAMessage = CreateWindow(TEXT("EDIT"),szMessageBuffer,WS_VISIBLE|WS_CHILD|ES_MULTILINE |ES_CENTER|WS_BORDER|ES_READONLY,
                                 0, 180,240,50,hWndMapWindow,NULL,hInst,NULL);

      ShowWindow(hAMessage,SW_SHOW);
      UpdateWindow(hAMessage);

      LastAi = i;
      return;
    }
  else if(hAMessage)
    {
      DestroyWindow(hAMessage);
      hAMessage = NULL;
      LastAi = -1;
    }
  else
    {
      LastAi = -1;
    }
}

void AATStats(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  double Temp;
  int i;
  double MaxDistance, MinDistance;
  double LegToGo, LegDistance;
  double TaskAltitudeRequired = 0;

  if(!AATEnabled)
    {
      return;
    }

  Temp = Basic->Time - Calculated->TaskStartTime;
  if((Temp >=0)&&(ActiveWayPoint >0))
    {
      Calculated->AATTimeToGo = (AATTaskLength*60) - Temp;
      if(Calculated->AATTimeToGo <= 0)
        Calculated->AATTimeToGo = 0;
      if(Calculated->AATTimeToGo >= (AATTaskLength * 60) )
        Calculated->AATTimeToGo = (AATTaskLength * 60);
    }

  MaxDistance = 0; MinDistance = 0;
  // Calculate Task Distances

  Calculated->TaskDistanceToGo = 0;
  // JMW: not sure why this is here?

  if(ActiveWayPoint >=0)
    {
      i=ActiveWayPoint;

      LegToGo = Distance(Basic->Lattitude , Basic->Longditude ,
                         WayPointList[Task[i].Index].Lattitude,
                         WayPointList[Task[i].Index].Longditude);

      if(Task[ActiveWayPoint].AATType == CIRCLE)
        {
          MaxDistance = LegToGo + (Task[i].AATCircleRadius * 2);
          MinDistance = LegToGo - (Task[i].AATCircleRadius * 2);
        }
      else
        {
          MaxDistance = LegToGo + (Task[ActiveWayPoint].AATSectorRadius * 2);
          MinDistance = LegToGo;
        }

      i++;
      while((Task[i].Index != -1) && (i<MAXTASKPOINTS))
        {
          LegDistance = Distance(WayPointList[Task[i].Index].Lattitude,
                                 WayPointList[Task[i].Index].Longditude,
                                 WayPointList[Task[i-1].Index].Lattitude,
                                 WayPointList[Task[i-1].Index].Longditude);

          if(Task[ActiveWayPoint].AATType == CIRCLE)
            {
              MaxDistance += LegDistance + (Task[i].AATCircleRadius * 2);
              MinDistance += LegDistance- (Task[i].AATCircleRadius * 2);
            }
          else
            {
              MaxDistance += LegDistance + (Task[ActiveWayPoint].AATSectorRadius * 2);
              MinDistance += LegDistance;
            }
          i++;
        }
      Calculated->AATMaxDistance = MaxDistance;
      Calculated->AATMinDistance = MinDistance;
      if(Calculated->AATTimeToGo >0)
        {
          Calculated->AATMaxSpeed = Calculated->AATMaxDistance / Calculated->AATTimeToGo;
          Calculated->AATMinSpeed = Calculated->AATMinDistance / Calculated->AATTimeToGo;
        }
    }
}


void ThermalBand(NMEA_INFO *Basic, DERIVED_INFO *Calculated)
{
  static double LastTime = 0;
  if(Basic->Time <= LastTime)
    {
      LastTime = Basic->Time;
      return;
    }
  LastTime = Basic->Time;


  // JMW TODO: Should really work out dt here, but i'm assuming constant time steps
  double dheight = Basic->Altitude-SAFETYALTITUDEBREAKOFF;

  int index, i, j;

  if (dheight<0) {
    return; // nothing to do.
  }
  if (Calculated->MaxThermalHeight==0) {
    Calculated->MaxThermalHeight = dheight;
  }

  // only do this if in thermal and have been climbing
  if ((!Calculated->Circling)||(Calculated->Average30s<0)) return;

  if (dheight > Calculated->MaxThermalHeight) {

  if (EnableSoundTask) {
    PlayResource(TEXT("IDR_WAV_BEEPBWEEP"));
  }

    // moved beyond ceiling, so redistribute buckets
    double mthnew;
    double tmpW[NUMTHERMALBUCKETS];
    int tmpN[NUMTHERMALBUCKETS];
    double h;

    // calculate new buckets so glider is below max
    double hbuk = Calculated->MaxThermalHeight/NUMTHERMALBUCKETS;

    mthnew = Calculated->MaxThermalHeight;
    while (mthnew<dheight) {
      mthnew += hbuk;
    }

    // reset counters
    for (i=0; i<NUMTHERMALBUCKETS; i++) {
      tmpW[i]= 0.0;
      tmpN[i]= 0;
    }
    // shift data into new buckets
    for (i=0; i<NUMTHERMALBUCKETS; i++) {
      h = (i)*(Calculated->MaxThermalHeight)/(NUMTHERMALBUCKETS); // height of center of bucket
      j = iround(NUMTHERMALBUCKETS*h/mthnew);

      //      h = (i)*(mthnew)/(NUMTHERMALBUCKETS); // height of center of bucket
      //      j = iround(NUMTHERMALBUCKETS*h/Calculated->MaxThermalHeight);

      if (j<NUMTHERMALBUCKETS) {
        if (Calculated->ThermalProfileN[i]>0) {
          tmpW[j] += Calculated->ThermalProfileW[i];
          tmpN[j] += Calculated->ThermalProfileN[i];
        }
      }
    }
    for (i=0; i<NUMTHERMALBUCKETS; i++) {
      Calculated->ThermalProfileW[i]= tmpW[i];
      Calculated->ThermalProfileN[i]= tmpN[i];
    }
    Calculated->MaxThermalHeight= mthnew;
  }

  index = iround(NUMTHERMALBUCKETS*(dheight/Calculated->MaxThermalHeight));
  if (index==NUMTHERMALBUCKETS) {
    index= NUMTHERMALBUCKETS-1;
  }

  Calculated->ThermalProfileW[index]+= Calculated->Vario;
  Calculated->ThermalProfileN[index]++;

}



//////////////////////////////////////////////////////////
// Final glide through terrain and footprint calculations


double FinalGlideThroughTerrain(double bearing, NMEA_INFO *Basic,
                                DERIVED_INFO *Calculated,
                                double *retlat, double *retlon)
{

  // returns distance one would arrive at altitude in straight glide

  // first estimate max range at this altitude
  double ialtitude = McReadyAltitude(MACREADY/LIFTMODIFY,
                                     1.0, bearing,
                                     Calculated->WindSpeed,
                                     Calculated->WindBearing, 0, 0, 1);
  double maxrange = Basic->Altitude/ialtitude;
  double lat, lon;
  double latlast, lonlast;
  double h=0.0, dh=0.0;
  int imax=0;
  double dhlast=0;
  double distance, altitude;
  double distancelast=0;

  if (retlat && retlon) {
    *retlat = Basic->Lattitude;
    *retlon = Basic->Longditude;
  }

  // calculate terrain rounding factor
  LockTerrainData();
  terrain_dem.SetTerrainRounding(maxrange/NUMFINALGLIDETERRAIN/1000.0);

  for (int i=0; i<=NUMFINALGLIDETERRAIN; i++) {
    distance = i*maxrange/NUMFINALGLIDETERRAIN;
    altitude = (NUMFINALGLIDETERRAIN-i)*(Basic->Altitude)/NUMFINALGLIDETERRAIN;

    // find lat, lon of point of interest

    lat = FindLattitude(Basic->Lattitude, Basic->Longditude, bearing, distance);
    lon = FindLongditude(Basic->Lattitude, Basic->Longditude, bearing, distance);

    // find height over terrain
    h =  terrain_dem.GetTerrainHeight(lat, lon); // latitude, longitude
    dh = altitude - h -  SAFETYALTITUDETERRAIN;
    //SAFETYALTITUDEARRIVAL;

    if ((dh<=0)&&(dhlast>0)) {
      double f = (0.0-dhlast)/(dh-dhlast);
      if (retlat && retlon) {
        *retlat = latlast*(1.0-f)+lat*f;
        *retlon = lonlast*(1.0-f)+lon*f;
      }
      UnlockTerrainData();
      return distancelast*(1.0-f)+distance*(f);
    }
    if (i&&(distance<= 0.0)) {
      UnlockTerrainData();
      return 0.0;
    }

    distancelast = distance;
    dhlast = dh;
    latlast = lat;
    lonlast = lon;
  }
  UnlockTerrainData();
  return 0.0;
}


//////////////////////////////////////////////////////////////////


double CalculateWaypointArrivalAltitude(NMEA_INFO *Basic,
					DERIVED_INFO *Calculated,
					int i) {
  double AltReqd;
  double wDistance, wBearing;

  wDistance = Distance(Basic->Lattitude,
		       Basic->Longditude,
		       WayPointList[i].Lattitude,
		       WayPointList[i].Longditude);

  wBearing = Bearing(Basic->Lattitude,
		     Basic->Longditude,
		     WayPointList[i].Lattitude,
		     WayPointList[i].Longditude);

  AltReqd = McReadyAltitude(0.0,
			    wDistance,
			    wBearing,
			    Calculated->WindSpeed,
			    Calculated->WindBearing,
			    0,
			    0,
			    1
			    )*(1/BUGS);

  return ((Basic->Altitude) - AltReqd - WayPointList[i].Altitude);
}



void SortLandableWaypoints(NMEA_INFO *Basic,
			   DERIVED_INFO *Calculated) {
  int SortedLandableIndex[MAXTASKPOINTS];
  double SortedArrivalAltitude[MAXTASKPOINTS];
  int i, k, l;
  double aa;

  for (i=0; i<MAXTASKPOINTS; i++) {
    SortedLandableIndex[i]= -1;
    SortedArrivalAltitude[i] = 0;
  }

  for (i=0; i<(int)NumberOfWayPoints; i++) {

    if (!(
	  ((WayPointList[i].Flags & AIRPORT) == AIRPORT)
	  || ((WayPointList[i].Flags & LANDPOINT) == LANDPOINT))) {
      continue; // ignore non-landable fields
    }

    aa = CalculateWaypointArrivalAltitude(Basic, Calculated, i);

    // see if this fits into slot
    for (k=0; k< MAXTASKPOINTS; k++) {
      if (
	  ((aa > SortedArrivalAltitude[k]) // closer than this one
	   ||(SortedLandableIndex[k]== -1)) // or this one isn't filled
	  &&(SortedLandableIndex[k]!= i) // and not replacing with same
	  )
	{

	  // ok, got new biggest, put it into the slot.

	  for (l=MAXTASKPOINTS-1; l>k; l--) {
	    if (l>0) {
	      SortedArrivalAltitude[l] = SortedArrivalAltitude[l-1];
	      SortedLandableIndex[l] = SortedLandableIndex[l-1];
	    }
	  }

	  SortedArrivalAltitude[k] = aa;
	  SortedLandableIndex[k] = i;
	  k=MAXTASKPOINTS;
	}
    }
  }

  // now we have a sorted list.

  // check if current waypoint is in the sorted list

  int foundActiveWayPoint = -1;
  for (i=0; i<MAXTASKPOINTS; i++) {
    if (ActiveWayPoint>=0) {
      if (SortedLandableIndex[i] == Task[ActiveWayPoint].Index) {
        foundActiveWayPoint = i;
      }
    }
  }

  if (foundActiveWayPoint != -1) {
    ActiveWayPoint = foundActiveWayPoint;
  } else {
    // if not found, set active waypoint to closest
    ActiveWayPoint = 0;
  }

  // set new waypoints in task

  for (i=0; i<MAXTASKPOINTS; i++) {
    Task[i].Index = SortedLandableIndex[i];
  }

}


void ResumeAbortTask() {
  static int OldTask[MAXTASKPOINTS];
  static int OldActiveWayPoint= -1;
  int i;

  TaskAborted = !TaskAborted;

  if (TaskAborted) {

    // save current task in backup

    for (i=0; i<MAXTASKPOINTS; i++) {
      OldTask[i]= Task[i].Index;
    }
    OldActiveWayPoint = ActiveWayPoint;

    // force new waypoint to be the closest
    ActiveWayPoint = -1;

  } else {

    // reload backup task

    for (i=0; i<MAXTASKPOINTS; i++) {
      Task[i].Index = OldTask[i];
    }
    ActiveWayPoint = OldActiveWayPoint;

    for (i=0; i<MAXTASKPOINTS; i++) {
      if (Task[i].Index != -1) {
        RefreshTaskWaypoint(i);
      }
    }
    CalculateTaskSectors();
    CalculateAATTaskSectors();

  }


}

