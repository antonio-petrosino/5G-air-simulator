/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 TELEMATICS LAB, Politecnico di Bari
 *
 * This file is part of 5G-air-simulator
 *
 * 5G-air-simulator is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation;
 *
 * 5G-air-simulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 5G-air-simulator; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Francesco Capozzi <f.capozzi@poliba.it>
 */

#include "../channel/RadioChannel.h"
#include "../phy/gnb-phy.h"
#include "../phy/ue-phy.h"
#include "../core/spectrum/bandwidth-manager.h"
#include "../networkTopology/Cell.h"
#include "../protocolStack/packet/packet-burst.h"
#include "../protocolStack/packet/Packet.h"
#include "../core/eventScheduler/simulator.h"
#include "../flows/application/InfiniteBuffer.h"
#include "../flows/application/VoIP.h"
#include "../flows/application/CBR.h"
#include "../flows/application/TraceBased.h"
#include "../device/IPClassifier/ClassifierParameters.h"
#include "../flows/QoS/QoSParameters.h"
#include "../flows/QoS/QoSForEXP.h"
#include "../flows/QoS/QoSForFLS.h"
#include "../flows/QoS/QoSForM_LWDF.h"
#include "../componentManagers/FrameManager.h"
#include "../utility/RandomVariable.h"
#include "../utility/UsersDistribution.h"
#include "../utility/IndoorScenarios.h"
#include "../load-parameters.h"
#include "../device/HeNodeB.h"
#include <iostream>
#include <queue>
#include <fstream>
#include <stdlib.h>
#include <cstring>


static void SingleCellWithFemto (int argc, char *argv[])
{
  double radius = atof(argv[2]);
  int nbBuildings = atoi(argv[3]);
  int buildingType = atoi(argv[4]);
  double activityRatio = atof(argv[5]);
  int nbUE = atoi(argv[6]);
  int nbFemtoUE = atoi(argv[7]);
  int nbVoIP = atoi(argv[8]);
  int nbVideo = atoi(argv[9]);
  int nbBE = atoi(argv[10]);
  int nbCBR = atoi(argv[11]);
  int sched_type = atoi(argv[12]);
  int frame_struct = atoi(argv[13]);
  int speed = atoi(argv[14]);
  int accessPolicy = atoi(argv[15]);
  double maxDelay = atof(argv[16]);
  int videoBitRate = atoi(argv[17]);
  int seed;
  if (argc==19)
    {
      seed = atoi(argv[18]);
    }
  else
    {
      seed = -1;
    }

  int nbCell = 1;

  // define simulation times
  double duration = 30;
  double flow_duration = 20;

  int cluster = 3;
  double bandwidth = 5;

  // CREATE COMPONENT MANAGER
  Simulator *simulator = Simulator::Init();
  FrameManager *frameManager = FrameManager::Init();
  NetworkManager* nm = NetworkManager::Init();

  // CONFIGURE SEED
  if (seed >= 0)
    {
      srand (seed);
    }
  else
    {
      srand (time(nullptr));
    }
  cout << "Simulation with SEED = " << seed << endl;

  // SET SCHEDULING ALLOCATION SCHEME
  GNodeB::DLSchedulerType downlink_scheduler_type;
  switch (sched_type)
    {
    case 1:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_PROPORTIONAL_FAIR;
      cout << "Scheduler PF "<< endl;
      break;
    case 2:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_MLWDF;
      cout << "Scheduler MLWDF "<< endl;
      break;
    case 3:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_EXP;
      cout << "Scheduler EXP "<< endl;
      break;
    case 4:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_FLS;
      cout << "Scheduler FLS "<< endl;
      break;
    case 5:
      downlink_scheduler_type = GNodeB::DLScheduler_EXP_RULE;
      cout << "Scheduler EXP_RULE "<< endl;
      break;
    case 6:
      downlink_scheduler_type = GNodeB::DLScheduler_LOG_RULE;
      cout << "Scheduler LOG RULE "<< endl;
      break;
    case 7:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_MAXIMUM_THROUGHPUT;
      cout << "Scheduler MT "<< endl;
      break;
    case 8:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_ROUND_ROBIN;
      cout << "Scheduler RR "<< endl;
      break;
    default:
      downlink_scheduler_type = GNodeB::DLScheduler_TYPE_PROPORTIONAL_FAIR;
      break;
    }

  // SET FRAME STRUCTURE
  FrameManager::FrameStructure frame_structure;
  switch (frame_struct)
    {
    case 1:
      frame_structure = FrameManager::FRAME_STRUCTURE_FDD;
      break;
    case 2:
      frame_structure = FrameManager::FRAME_STRUCTURE_TDD;
      break;
    default:
      cout << "Error: invalid frame structure specified!" << endl;
      exit(1);
    }
  frameManager->SetFrameStructure(frame_structure);




  //create macro-cells
  vector <Cell*> *cells = new vector <Cell*>;
  for (int i = 0; i < nbCell; i++)
    {
      CartesianCoordinates center =
        GetCartesianCoordinatesForCell(i, radius * 1000.);

      Cell *c = new Cell (i, radius, 0.035, center.GetCoordinateX (), center.GetCoordinateY ());
      cells->push_back (c);
      nm->GetCellContainer ()->push_back (c);

      cout << "Created Cell, id " << c->GetIdCell ()
                <<", position: " << c->GetCellCenterPosition ()->GetCoordinateX ()
                << " " << c->GetCellCenterPosition ()->GetCoordinateY () << endl;
    }

  //////////////////////
  //create femto-cells
  //
  int femtoCellsInBuilding = 1;
  if ( buildingType == 0 )
    {
      femtoCellsInBuilding = 25;
    }
  else
    {
      femtoCellsInBuilding = 40;
    }
  int nbFemtoCells = nbBuildings * femtoCellsInBuilding;
  int firstFemtoinBuildingID = nbCell;
  int apartmentSide = 10; //[m]
  int nbFloors = 1;
  //users are distributed uniformly into a cell
  vector<CartesianCoordinates*> *building_positions = GetUniformBuildingDistribution (0, nbBuildings);

  for (int idBuilding = 0; idBuilding < nbBuildings; idBuilding++)
    {
      // Get Building Positions
      double buildingCenter_X = building_positions->at (idBuilding)->GetCoordinateX ();
      double buildingCenter_Y = building_positions->at (idBuilding)->GetCoordinateY ();

      nm->CreateBuildingForFemtocells( idBuilding, buildingType, apartmentSide, nbFloors,
                                       buildingCenter_X, buildingCenter_Y,
                                       firstFemtoinBuildingID, femtoCellsInBuilding);

      cout << "Created Building, id " << idBuilding
                <<", position: " << buildingCenter_X
                << " " << buildingCenter_Y
                << " and " << nbFloors << " floors"
                << " and " << femtoCellsInBuilding << " femtocells" << endl;

      firstFemtoinBuildingID += femtoCellsInBuilding;
    }
  //
  //
  ///////////////////////////

  vector <BandwidthManager*> spectrums = RunFrequencyReuseTechniques (nbCell, cluster, bandwidth);

  BandwidthManager* femto_spectrums = spectrums.at(0);
// vector <BandwidthManager*> femto_spectrums = RunFrequencyReuseTechniques (nbCell, cluster, bandwidth);

  //Create a set of a couple of channels
  vector <RadioChannel*> *dlChannels = new vector <RadioChannel*>;
  vector <RadioChannel*> *ulChannels = new vector <RadioChannel*>;
  for (int i= 0; i < nbCell + nbFemtoCells; i++)
    {
      RadioChannel *dlCh = new RadioChannel ();
      dlCh->SetChannelId (i);
      dlChannels->push_back (dlCh);

      RadioChannel *ulCh = new RadioChannel ();
      ulCh->SetChannelId (i);
      ulChannels->push_back (ulCh);
    }


  //create gNBs
  vector <GNodeB*> *gNBs = new vector <GNodeB*>;
  for (int i = 0; i < nbCell; i++)
    {
      GNodeB* gnb = new GNodeB (i, cells->at (i));
      gnb->GetPhy ()->SetDlChannel (dlChannels->at (i));
      gnb->GetPhy ()->SetUlChannel (ulChannels->at (i));

      gnb->SetDLScheduler (downlink_scheduler_type);

      gnb->GetPhy ()->SetBandwidthManager (spectrums.at (i));

      cout << "Created gnb, id " << gnb->GetIDNetworkNode()
                << ", cell id " << gnb->GetCell ()->GetIdCell ()
                <<", position: " << gnb->GetMobilityModel ()->GetAbsolutePosition ()->GetCoordinateX ()
                << " " << gnb->GetMobilityModel ()->GetAbsolutePosition ()->GetCoordinateY ()
                << ", channels id " << gnb->GetPhy ()->GetDlChannel ()->GetChannelId ()
                << gnb->GetPhy ()->GetUlChannel ()->GetChannelId ()  << endl;

      spectrums.at (i)->Print ();


      ulChannels->at (i)->AddDevice(gnb);


      nm->GetGNodeBContainer ()->push_back (gnb);
      gNBs->push_back (gnb);
    }
  //create Home gNBs
  vector <Femtocell*> *femtocells = nm->GetFemtoCellContainer();
  for (int i = nbCell; i < nbCell + nbFemtoCells; i++)
    {
      double HeNBdrop = (double) rand()/ (double) RAND_MAX;
      HeNodeB* gnb = new HeNodeB (i, femtocells->at (i-nbCell));
      gnb->GetPhy ()->SetDlChannel (dlChannels->at (i));
      gnb->GetPhy ()->SetUlChannel (ulChannels->at (i));
      gnb->GetPhy ()->SetBandwidthManager (femto_spectrums);

      if (accessPolicy == 1)
        {
          gnb->GetMacEntity()->SetRestrictedAccessMode(false);
        }

      cout << "Created Home gnb, id " << gnb->GetIDNetworkNode()
                              << ", cell id " << gnb->GetCell ()->GetIdCell ()
                              <<", position: " << gnb->GetMobilityModel ()->GetAbsolutePosition ()->GetCoordinateX ()
                              << " " << gnb->GetMobilityModel ()->GetAbsolutePosition ()->GetCoordinateY ()
                              << ", channels id " << gnb->GetPhy ()->GetDlChannel ()->GetChannelId ()
                              << gnb->GetPhy ()->GetUlChannel ()->GetChannelId ();
      if ( HeNBdrop <= activityRatio )
              {
                gnb->SetDLScheduler (downlink_scheduler_type);
                cout << ", active " << endl;
              }
      else
        {
          cout << ", inactive " << endl;
        }
      femto_spectrums->Print ();

      ulChannels->at (i)->AddDevice(gnb);

      nm->GetHomeGNodeBContainer()->push_back (gnb);

    }
  vector <HeNodeB*> *HeNBs = nm->GetHomeGNodeBContainer();

  int totalNbUE = nbCell*nbUE + nbFemtoCells*nbFemtoUE;
  int totalNbCell = nbCell + nbFemtoCells;

  //Define Application Container
  VoIP VoIPApplication[ nbVoIP*totalNbUE ];
  TraceBased VideoApplication[ nbVideo*totalNbUE ];
  InfiniteBuffer BEApplication[ nbBE*totalNbUE ];
  CBR CBRApplication[ nbCBR*totalNbUE ];
  int voipApplication = 0;
  int videoApplication = 0;
  int cbrApplication = 0;
  int beApplication = 0;
  int destinationPort = 101;
  int applicationID = 0;

  //Create GW
  Gateway *gw = new Gateway ();
  nm->GetGatewayContainer ()->push_back (gw);


  // Users in MACRO CELL
  //nbUE is the number of users that are into each cell at the beginning of the simulation
  int idUE = totalNbCell;
  for (int j = 0; j < nbCell; j++)
    {

      //users are distributed uniformly into a cell
      vector<CartesianCoordinates*> *positions = GetUniformUsersDistribution (j, nbUE);

      //Create UEs
      for (int i = 0; i < nbUE; i++)
        {
          //ue's random position
          double posX = positions->at (idUE - totalNbCell)->GetCoordinateX ();
          double posY = positions->at (idUE - totalNbCell)->GetCoordinateY ();
          double speedDirection = (double)(rand() %360) * ((2*M_PI)/360);;

          UserEquipment* ue = new UserEquipment (idUE,
                                                 posX, posY, speed, speedDirection,
                                                 cells->at (j),
                                                 gNBs->at (j),
                                                 0, //HO deactivated!
                                                 Mobility::CONSTANT_POSITION);

          cout << "Created UE - id " << idUE << " position " << posX << " " << posY
                    << ", cell " <<  ue->GetCell ()->GetIdCell ()
                    << ", target gnb " << ue->GetTargetNode ()->GetIDNetworkNode () << endl;

          ue->GetPhy ()->SetDlChannel (gNBs->at (j)->GetPhy ()->GetDlChannel ());
          ue->GetPhy ()->SetUlChannel (gNBs->at (j)->GetPhy ()->GetUlChannel ());

          ue->SetIndoorFlag(false);

          FullbandCqiManager *cqiManager = new FullbandCqiManager ();
          cqiManager->SetCqiReportingMode (CqiManager::PERIODIC);
          cqiManager->SetReportingInterval (1);
          cqiManager->SetDevice (ue);
          ue->SetCqiManager (cqiManager);

          nm->GetUserEquipmentContainer ()->push_back (ue);

          // register ue to the gnb
          gNBs->at (j)->RegisterUserEquipment (ue);
          // define the channel realizations
          for (int k = 0; k < nbCell; k++)
            {
              ChannelRealization* c_dl = new ChannelRealization (gNBs->at (k), ue, ChannelRealization::CHANNEL_MODEL_MACROCELL_URBAN);
              gNBs->at (k)->GetPhy ()->GetDlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_dl);
              ChannelRealization* c_ul = new ChannelRealization (ue, gNBs->at (k), ChannelRealization::CHANNEL_MODEL_MACROCELL_URBAN);
              gNBs->at (k)->GetPhy ()->GetUlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_ul);
            }
          for (int k = 0; k < nbFemtoCells; k++)
            {
              ChannelRealization* c_dl = new ChannelRealization (HeNBs->at (k), ue, ChannelRealization::CHANNEL_MODEL_FEMTOCELL_URBAN);
              HeNBs->at (k)->GetPhy ()->GetDlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_dl);
              ChannelRealization* c_ul = new ChannelRealization (ue, HeNBs->at (k), ChannelRealization::CHANNEL_MODEL_FEMTOCELL_URBAN);
              HeNBs->at (k)->GetPhy ()->GetUlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_ul);
            }
          idUE++;
        }
    }

  // Users in FEMTO CELLS
  //nbUE is the number of users that are into each cell at the beginning of the simulation
  //idUE = nbCell*nbUE;
  for (int j = 0; j < nbFemtoCells; j++)
    {
      int idCell = j + nbCell;
      vector<CartesianCoordinates*> *positions = GetUniformUsersDistributionInFemtoCell (idCell, nbFemtoUE);

      //Create UEs
      for (int i = 0; i < nbFemtoUE; i++)
        {
          //ue's random position
          double posX = positions->at (i)->GetCoordinateX ();
          double posY = positions->at (i)->GetCoordinateY ();
          double speedDirection = (double)(rand() %360) * ((2*M_PI)/360);;

          UserEquipment* ue = new UserEquipment (idUE,
                                                 posX, posY, speed, speedDirection,
                                                 femtocells->at (j),
                                                 HeNBs->at (j),
                                                 0, //HO deactivated!
                                                 Mobility::CONSTANT_POSITION);

          cout << "Created UE in femto-cell - id " << idUE << " position " << posX << " " << posY
                    << ", cell " <<  ue->GetCell ()->GetIdCell ()
                    << ", target gnb " << ue->GetTargetNode ()->GetIDNetworkNode () << endl;

          ue->GetPhy ()->SetDlChannel (HeNBs->at (j)->GetPhy ()->GetDlChannel ());
          ue->GetPhy ()->SetUlChannel (HeNBs->at (j)->GetPhy ()->GetUlChannel ());

          ue->SetIndoorFlag(true);

          if (accessPolicy == 1)
            {
              // adding Users to the closed subscriber group
              HeNBs->at(j)->GetMacEntity()->AddSubscribedUser(ue);
            }

          FullbandCqiManager *cqiManager = new FullbandCqiManager ();
          cqiManager->SetCqiReportingMode (CqiManager::PERIODIC);
          cqiManager->SetReportingInterval (1);
          cqiManager->SetDevice (ue);
          ue->SetCqiManager (cqiManager);

          nm->GetUserEquipmentContainer ()->push_back (ue);

          // register ue to the gnb
          HeNBs->at (j)->RegisterUserEquipment (ue);
          // define the channel realizations
          for (int k = 0; k < nbCell; k++)
            {
              ChannelRealization* c_dl = new ChannelRealization (gNBs->at (k), ue, ChannelRealization::CHANNEL_MODEL_MACROCELL_URBAN);
              gNBs->at (k)->GetPhy ()->GetDlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_dl);
              ChannelRealization* c_ul = new ChannelRealization (ue, gNBs->at (k), ChannelRealization::CHANNEL_MODEL_MACROCELL_URBAN);
              gNBs->at (k)->GetPhy ()->GetUlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_ul);
            }
          for (int k = 0; k < nbFemtoCells; k++)
            {
              ChannelRealization* c_dl = new ChannelRealization (HeNBs->at (k), ue, ChannelRealization::CHANNEL_MODEL_FEMTOCELL_URBAN);
              HeNBs->at (k)->GetPhy ()->GetDlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_dl);
              ChannelRealization* c_ul = new ChannelRealization (ue, HeNBs->at (k), ChannelRealization::CHANNEL_MODEL_FEMTOCELL_URBAN);
              HeNBs->at (k)->GetPhy ()->GetUlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_ul);
            }
          idUE++;
        }
    }


  for (auto ue : *nm->GetUserEquipmentContainer ())
    {
      // CREATE DOWNLINK APPLICATION FOR THIS UE
      double start_time = 0.1; // + GetRandomVariable (5.);
      double duration_time = start_time + flow_duration;
      // *** voip application
      for (int j = 0; j < nbVoIP; j++)
        {
          // create application
          VoIPApplication[voipApplication].SetSource (gw);
          VoIPApplication[voipApplication].SetDestination (ue);
          VoIPApplication[voipApplication].SetApplicationID (applicationID);
          VoIPApplication[voipApplication].SetStartTime(start_time);
          VoIPApplication[voipApplication].SetStopTime(duration_time);

          // create qos parameters
          if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_FLS)
            {
              QoSForFLS *qos = new QoSForFLS ();
              qos->SetMaxDelay (maxDelay);
              if (maxDelay == 0.1)
                {
                  cout << "Target Delay = 0.1 s, M = 9" << endl;
                  qos->SetNbOfCoefficients (9);
                }
              else if (maxDelay == 0.08)
                {
                  cout << "Target Delay = 0.08 s, M = 7" << endl;
                  qos->SetNbOfCoefficients (7);
                }
              else if (maxDelay == 0.06)
                {
                  cout << "Target Delay = 0.06 s, M = 5" << endl;
                  qos->SetNbOfCoefficients (5);
                }
              else if (maxDelay == 0.04)
                {
                  cout << "Target Delay = 0.04 s, M = 3" << endl;
                  qos->SetNbOfCoefficients (3);
                }
              else
                {
                  cout << "ERROR: target delay is not available"<< endl;
                  return;
                }

              VoIPApplication[voipApplication].SetQoSParameters (qos);
            }
          else if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_EXP)
            {
              QoSForEXP *qos = new QoSForEXP ();
              qos->SetMaxDelay (maxDelay);
              VoIPApplication[voipApplication].SetQoSParameters (qos);
            }
          else if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_MLWDF)
            {
              QoSForM_LWDF *qos = new QoSForM_LWDF ();
              qos->SetMaxDelay (maxDelay);
              VoIPApplication[voipApplication].SetQoSParameters (qos);
            }
          else
            {
              QoSParameters *qos = new QoSParameters ();
              qos->SetMaxDelay (maxDelay);
              VoIPApplication[voipApplication].SetQoSParameters (qos);
            }


          //create classifier parameters
          ClassifierParameters *cp = new ClassifierParameters (gw->GetIDNetworkNode(),
              ue->GetIDNetworkNode(),
              0,
              destinationPort,
              TransportProtocol::TRANSPORT_PROTOCOL_TYPE_UDP);
          VoIPApplication[voipApplication].SetClassifierParameters (cp);

          cout << "CREATED VOIP APPLICATION, ID " << applicationID << endl;

          //update counter
          destinationPort++;
          applicationID++;
          voipApplication++;
        }


      // *** video application
      for (int j = 0; j < nbVideo; j++)
        {
          // create application
          VideoApplication[videoApplication].SetSource (gw);
          VideoApplication[videoApplication].SetDestination (ue);
          VideoApplication[videoApplication].SetApplicationID (applicationID);
          VideoApplication[videoApplication].SetStartTime(start_time);
          VideoApplication[videoApplication].SetStopTime(duration_time);

          switch (videoBitRate)
            {
            case 128:
              {
                VideoApplication[videoApplication].LoadInternalTrace(&foreman_h264_128k);
//                  VideoApplication[videoApplication].LoadInternalTrace(&highway_h264_128k);
//                  VideoApplication[videoApplication].LoadInternalTrace(&mobile_h264_128k);
                cout << "  selected video @ 128k"<< endl;
                break;
              }
            case 242:
              {
                VideoApplication[videoApplication].LoadInternalTrace(&foreman_h264_242k);
                cout << "  selected video @ 242k"<< endl;
                break;
              }
            case 440:
              {
                VideoApplication[videoApplication].LoadInternalTrace(&foreman_h264_440k);
                cout << "  selected video @ 440k"<< endl;
                break;
              }
            default:
              {
                cout << "  Unsupported video bitrate!"<< endl;
                exit(1);
              }
            }

          // create qos parameters
          if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_FLS)
            {
              QoSForFLS *qos = new QoSForFLS ();
              qos->SetMaxDelay (maxDelay);
              if (maxDelay == 0.1)
                {
                  cout << "Target Delay = 0.1 s, M = 9" << endl;
                  qos->SetNbOfCoefficients (9);
                }
              else if (maxDelay == 0.08)
                {
                  cout << "Target Delay = 0.08 s, M = 7" << endl;
                  qos->SetNbOfCoefficients (7);
                }
              else if (maxDelay == 0.06)
                {
                  cout << "Target Delay = 0.06 s, M = 5" << endl;
                  qos->SetNbOfCoefficients (5);
                }
              else if (maxDelay == 0.04)
                {
                  cout << "Target Delay = 0.04 s, M = 3" << endl;
                  qos->SetNbOfCoefficients (3);
                }
              else
                {
                  cout << "ERROR: target delay is not available"<< endl;
                  return;
                }

              VideoApplication[videoApplication].SetQoSParameters (qos);
            }
          else if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_EXP)
            {
              QoSForEXP *qos = new QoSForEXP ();
              qos->SetMaxDelay (maxDelay);
              VideoApplication[videoApplication].SetQoSParameters (qos);
            }
          else if (downlink_scheduler_type == GNodeB::DLScheduler_TYPE_MLWDF)
            {
              QoSForM_LWDF *qos = new QoSForM_LWDF ();
              qos->SetMaxDelay (maxDelay);
              VideoApplication[videoApplication].SetQoSParameters (qos);
            }
          else
            {
              QoSParameters *qos = new QoSParameters ();
              qos->SetMaxDelay (maxDelay);
              VideoApplication[videoApplication].SetQoSParameters (qos);
            }


          //create classifier parameters
          ClassifierParameters *cp = new ClassifierParameters (gw->GetIDNetworkNode(),
              ue->GetIDNetworkNode(),
              0,
              destinationPort,
              TransportProtocol::TRANSPORT_PROTOCOL_TYPE_UDP);
          VideoApplication[videoApplication].SetClassifierParameters (cp);

          cout << "CREATED VIDEO APPLICATION, ID " << applicationID << endl;

          //update counter
          destinationPort++;
          applicationID++;
          videoApplication++;
        }

      // *** be application
      for (int j = 0; j < nbBE; j++)
        {
          // create application
          BEApplication[beApplication].SetSource (gw);
          BEApplication[beApplication].SetDestination (ue);
          BEApplication[beApplication].SetApplicationID (applicationID);
          BEApplication[beApplication].SetStartTime(start_time);
          BEApplication[beApplication].SetStopTime(duration_time);


          // create qos parameters
          QoSParameters *qosParameters = new QoSParameters ();
          BEApplication[beApplication].SetQoSParameters (qosParameters);


          //create classifier parameters
          ClassifierParameters *cp = new ClassifierParameters (gw->GetIDNetworkNode(),
              ue->GetIDNetworkNode(),
              0,
              destinationPort,
              TransportProtocol::TRANSPORT_PROTOCOL_TYPE_UDP);
          BEApplication[beApplication].SetClassifierParameters (cp);

          cout << "CREATED BE APPLICATION, ID " << applicationID << endl;

          //update counter
          destinationPort++;
          applicationID++;
          beApplication++;
        }

      // *** cbr application
      for (int j = 0; j < nbCBR; j++)
        {
          // create application
          CBRApplication[cbrApplication].SetSource (gw);
          CBRApplication[cbrApplication].SetDestination (ue);
          CBRApplication[cbrApplication].SetApplicationID (applicationID);
          CBRApplication[cbrApplication].SetStartTime(start_time);
          CBRApplication[cbrApplication].SetStopTime(duration_time);
          CBRApplication[cbrApplication].SetInterval (0.04);
          CBRApplication[cbrApplication].SetSize (5);

          // create qos parameters
          QoSParameters *qosParameters = new QoSParameters ();
          qosParameters->SetMaxDelay (maxDelay);

          CBRApplication[cbrApplication].SetQoSParameters (qosParameters);


          //create classifier parameters
          ClassifierParameters *cp = new ClassifierParameters (gw->GetIDNetworkNode(),
              ue->GetIDNetworkNode(),
              0,
              destinationPort,
              TransportProtocol::TRANSPORT_PROTOCOL_TYPE_UDP);
          CBRApplication[cbrApplication].SetClassifierParameters (cp);

          cout << "CREATED CBR APPLICATION, ID " << applicationID << endl;

          //update counter
          destinationPort++;
          applicationID++;
          cbrApplication++;
        }



    }


  simulator->SetStop(duration);
  simulator->Run ();

}
