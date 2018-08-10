////////////////////////////////////////////////////////////////////////////////
/// \file CRTDetSim_module.cc
///
/// Based on LArIAT TOFSimDigits.cc (Author: Lucas Mendes Santos)
/// with modifications for SBND (Author: mastbaum@uchicago.edu)
/// then modified for ICARUS
///
/// Author: Chris.Hilgenberg@colostate.edu
////////////////////////////////////////////////////////////////////////////////

//art includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "nutools/RandomUtils/NuRandomService.h"

//larsoft includes
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardataalg/DetectorInfo/ElecClock.h"
#include "lardataobj/Simulation/AuxDetSimChannel.h"
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/AuxDetGeo.h"
#include "larcore/Geometry/AuxDetGeometry.h"
#include "larcorealg/Geometry/CryostatGeo.h"
#include "larcorealg/CoreUtils/NumericUtils.h"

//CLHEP includes
#include "CLHEP/Random/RandomEngine.h"
#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandGauss.h"
#include "CLHEP/Random/RandPoisson.h"

//ROOT includes
#include "TFile.h"
#include "TNtuple.h"
#include "TGeoManager.h"
#include "TGeoNode.h"

//C++ includes
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <map> 

//CRT includes
#include "icaruscode/CRT/CRTDetSim.h"
#include "icaruscode/CRT/CRTProducts/CRTChannelData.h"
#include "icaruscode/CRT/CRTProducts/CRTData.hh"

namespace icarus {
namespace crt {

void CRTDetSim::reconfigure(fhicl::ParameterSet const & p) {
  fVerbose = p.get<bool>("Verbose");
  fG4ModuleLabel = p.get<std::string>("G4ModuleLabel");
  fGlobalT0Offset = p.get<double>("GlobalT0Offset");
  fTDelayNorm = p.get<double>("TDelayNorm");
  fTDelayShift = p.get<double>("TDelayShift");
  fTDelaySigma = p.get<double>("TDelaySigma");
  fTDelayOffset = p.get<double>("TDelayOffset");
  fTDelayRMSGausNorm = p.get<double>("TDelayRMSGausNorm");
  fTDelayRMSGausShift = p.get<double>("TDelayRMSGausShift");
  fTDelayRMSGausSigma = p.get<double>("TDelayRMSGausSigma");
  fTDelayRMSExpNorm = p.get<double>("TDelayRMSExpNorm");
  fTDelayRMSExpShift = p.get<double>("TDelayRMSExpShift");
  fTDelayRMSExpScale = p.get<double>("TDelayRMSExpScale");
  fPropDelay = p.get<double>("PropDelay");
  fPropDelayError = p.get<double>("PropDelayError");
  fTResInterpolator = p.get<double>("TResInterpolator");
  fUseEdep = p.get<bool>("UseEdep");
  fQ0 = p.get<double>("Q0");
  fQPed = p.get<double>("QPed");
  fQSlope = p.get<double>("QSlope");
  fQRMS = p.get<double>("QRMS");
  fQThresholdC = p.get<double>("QThresholdC");
  fQThresholdM = p.get<double>("QThresholdM");
  fQThresholdD = p.get<double>("QThresholdD");
  fStripCoincidenceWindow = p.get<double>("StripCoincidenceWindow");
  fApplyCoincidenceC = p.get<bool>("ApplyCoincidenceC");
  fApplyCoincidenceM = p.get<bool>("ApplyCoincidenceM");
  fApplyCoincidenceD = p.get<bool>("ApplyCoincidenceD");
  fLayerCoincidenceWindowC = p.get<double>("LayerCoincidenceWindowC");
  fLayerCoincidenceWindowM = p.get<double>("LayerCoincidenceWindowM");
  fLayerCoincidenceWindowD = p.get<double>("LayerCoincidenceWindowD");
  //fAbsLenEffC = p.get<double>("AbsLenEffC");
  //fAbsLenEffM = p.get<double>("AbsLenEffM");
  //fAbsLenEffD = p.get<double>("AbsLenEffD");
  fDeadTime = p.get<double>("DeadTime");
  fBiasTime = p.get<double>("BiasTime");
}

// constructor
CRTDetSim::CRTDetSim(fhicl::ParameterSet const & p) {
  art::ServiceHandle<rndm::NuRandomService> seeds;
  seeds->createEngine(*this, "HepJamesRandom", "crt", p, "Seed");

  this->reconfigure(p);

  produces<std::vector<icarus::crt::CRTData> >();
}

//function takes reference to AuxDetGeo object and gives parent subsystem
char CRTDetSim::GetAuxDetType(geo::AuxDetGeo const& adgeo)
{
  std::string volName(adgeo.TotalVolume()->GetName());
  if (volName.find("MINOS") != std::string::npos) return 'm';
  if (volName.find("CERN")  != std::string::npos) return 'c';
  if (volName.find("DC")    != std::string::npos) return 'd';

  mf::LogInfo("CRT") << "AuxDetType not found!" << '\n';
  return 'e';
}

//function takes reference to AuxDetGeo object and gives crt region
std::string CRTDetSim::GetAuxDetRegion(geo::AuxDetGeo const& adgeo)
{
  char type = CRTDetSim::GetAuxDetType(adgeo);
  std::string base = "volAuxDet_";
  switch ( type ) {
    case 'c' : base+= "CERN"; break;
    case 'd' : base+= "DC"; break;
    case 'm' : base+= "MINOS"; break;
  }
  base+="_module_###_";
  std::string volName(adgeo.TotalVolume()->GetName());

  return volName.substr(base.size(),volName.size());
}

uint32_t GetAuxDetRegionNum(std::string reg)
{
    if(reg == "Top")        return 38;
    if(reg == "SlopeLeft")  return 52;
    if(reg == "SlopeRight") return 56;
    if(reg == "SlopeFront") return 48;
    if(reg == "SlopeBack")  return 46;
    if(reg == "Left")       return 50;
    if(reg == "Right")      return 54;
    if(reg == "Front")      return 44;
    if(reg == "Back")       return 42;
    if(reg == "Bottom")     return 58;
    return UINT32_MAX;
}

//function for simulating time response
//  takes true hit time, LY(PE) observed, and longitudinal distance from readout
//  uses 12 FHiCL configurable parameters
//  returns simulated time in units of clock ticks
uint32_t CRTDetSim::GetChannelTriggerTicks(CLHEP::HepRandomEngine* engine,
                                         detinfo::ElecClock& clock,
                                         float t0, float npeMean, float r) {
  // Hit timing, with smearing and NPE dependence
  double tDelayMean = \
    fTDelayNorm *
      exp(-0.5 * pow((npeMean - fTDelayShift) / fTDelaySigma, 2)) +
    fTDelayOffset;

  double tDelayRMS = \
    fTDelayRMSGausNorm *
      exp(-pow(npeMean - fTDelayRMSGausShift, 2) / fTDelayRMSGausSigma) +
    fTDelayRMSExpNorm *
      exp(-(npeMean - fTDelayRMSExpShift) / fTDelayRMSExpScale);

  double tDelay = CLHEP::RandGauss::shoot(engine, tDelayMean, tDelayRMS);

  // Time resolution of the interpolator
  tDelay += CLHEP::RandGauss::shoot(engine, 0, fTResInterpolator);

  // Propagation time
  double tProp = CLHEP::RandGauss::shoot(fPropDelay, fPropDelayError) * r;

  double t = t0 + tProp + tDelay;

  // Get clock ticks
  clock.SetTime(t / 1e3);  // SetTime takes microseconds

  //if (fVerbose) mf::LogInfo("CRT")
  //  << "CRT TIMING: t0=" << t0
  //  << ", tDelayMean=" << tDelayMean << ", tDelayRMS=" << tDelayRMS
  //  << ", tDelay=" << tDelay << ", tDelay(interp)="
  //  << tDelay << ", tProp=" << tProp << ", t=" << t << ", ticks=" << clock.Ticks() << "\n"; 

  return clock.Ticks();
}

bool TimeOrderCRTData(icarus::crt::CRTChannelData crtdat1, icarus::crt::CRTChannelData crtdat2) {
    return ( crtdat1.T0() < crtdat2.T0() );
}

struct Tagger {
  char type;
  std::string reg; //crt region where FEB is located
  uint32_t stackid; //which module stack (applies to left/right m modules only)
  std::set<uint32_t> layerid; //keep track of layers hit accross whole event window
  std::map<uint32_t,uint32_t> chanlayer; //map chan # to layer
  std::pair<uint32_t,uint32_t> macPair; //which two FEBs provided coincidence (applies to m mods only)
  std::vector<icarus::crt::CRTChannelData> data; //time and charge info for each channel > thresh
};


//module producer
void CRTDetSim::produce(art::Event & e) {
  // A list of hit taggers, before any coincidence requirement
  std::map<uint32_t, Tagger> taggers;

  // Services: Geometry, DetectorClocks, RandomNumberGenerator
  art::ServiceHandle<geo::Geometry> geoService;
  art::ServiceHandle<detinfo::DetectorClocksService> detClocks;
  detinfo::ElecClock trigClock = detClocks->provider()->TriggerClock();

  art::ServiceHandle<art::RandomNumberGenerator> rng;
  CLHEP::HepRandomEngine* engine = &rng->getEngine("crt");

  // Handle for (truth) AuxDetSimChannels
  art::Handle<std::vector<sim::AuxDetSimChannel> > channels;
  e.getByLabel(fG4ModuleLabel, channels);

  uint32_t nsim_m=0, nsim_d=0, nsim_c=0;
  uint32_t nchandat_m=0, nchandat_d=0, nchandat_c=0;
  uint32_t nmissthr_c = 0, nmissthr_d = 0, nmissthr_m = 0;
  uint32_t nmiss_strcoin_c = 0;

  std::map<uint32_t,uint32_t> regCounts;
  std::set<uint32_t> regions;

  // Loop through truth AD channels
  for (auto& adsc : *channels) {

    uint32_t adid = adsc.AuxDetID(); //CRT module ID number (from gdml)
    uint32_t adsid = adsc.AuxDetSensitiveID(); //CRT strip ID number (from gdml)
    if (adsid == 0 ) continue; //skip AuxDetSensitiveID=0 (bug in AuxDetSimChannels)

    const geo::AuxDetGeo& adGeo = geoService->AuxDet(adid); //pointer to module object

    //check stripID is consistent with number of sensitive volumes
    if( adGeo.NSensitiveVolume() < adsid){
        std::cout << "adsID out of bounds! Skipping..." << "\n"
                  << "   " << adGeo.Name()  << " / modID "   << adid
                  << " / stripID " << adsid 
        << std::endl;
        continue;
    }

    const geo::AuxDetSensitiveGeo& adsGeo = adGeo.SensitiveVolume(adsid); //pointer to strip object
    char auxDetType = GetAuxDetType(adGeo); //CRT module type (c, d, or m)
    if (auxDetType=='e') mf::LogInfo("CRT") << "COULD NOT GET AD TYPE!" << '\n';
    std::string region = GetAuxDetRegion(adGeo); //CRT region

    uint32_t layid = UINT32_MAX; //set to 0 or 1 if layerid determined
    uint32_t stackid = UINT32_MAX; //for left/right crt regions ordered from down to upstream (-z -> +z)
    uint32_t mac5=UINT32_MAX;

    // Find the path to the strip geo node, to locate it in the hierarchy
    std::set<std::string> volNames = { adsGeo.TotalVolume()->GetName() };
    std::vector<std::vector<TGeoNode const*> > paths = geoService->FindAllVolumePaths(volNames);

    std::string path = "";
    for (size_t inode=0; inode<paths.at(0).size(); inode++) {
      path += paths.at(0).at(inode)->GetName();
      if (inode < paths.at(0).size() - 1) {
        path += "/";
      }
    }

    TGeoManager* manager = geoService->ROOTGeoManager();
    manager->cd(path.c_str());
    TGeoNode* nodeStrip = manager->GetCurrentNode();
    TGeoNode* nodeInner = manager->GetMother(1);
    TGeoNode* nodeModule = manager->GetMother(2);
    double origin[3] = {0, 0, 0};

    // Module position in parent (tagger) frame
    double modulePosMother[3]; //position in CRT region volume
    nodeModule->LocalToMaster(origin, modulePosMother);

    // strip position in module frame
    double stripPosMother[3];
    double stripPosModule[3];
    nodeStrip->LocalToMaster(origin, stripPosMother);
    nodeInner->LocalToMaster(stripPosMother,stripPosModule);

    // Determine layid and stackid
    if (auxDetType == 'c' || auxDetType == 'd') layid = (stripPosModule[1] > 0);
       
    if (auxDetType == 'm') {
      if ( region=="Left" || region=="Right" ) {
          if ( modulePosMother[2] < 0 ) stackid = 0;
          if ( modulePosMother[2] == 0) stackid = 1;
          if ( modulePosMother[2] > 0 ) stackid = 2;

          //following 2 if's use hardcoded dimensions - UPDATE AFTER ALL GEO CHANGES!
          if ( stackid == 0 || stackid == 2 ) layid = ( abs(modulePosMother[0]) < 49.482/2-1 );
          if ( stackid == 1 ) layid = ( abs(modulePosMother[0]) > 49.482/2-1 );
      }
      if ( region=="Front" || region=="Back" ) {
          layid = ( modulePosMother[2]> 0 );
      }
    }

    if(layid==UINT32_MAX) mf::LogInfo("CRT") << "layid NOT SET!!!" << '\n'
                               << "   ADType: " << auxDetType << '\n'
                               << "   ADRegion: " << region << '\n';

    // Simulate the CRT response for each hit
    for (auto ide : adsc.AuxDetIDEs()) {

      if (auxDetType=='c') nsim_c++;
      if (auxDetType=='d') nsim_d++;
      if (auxDetType=='m') nsim_m++;

      // What is the distance from the hit (centroid of the entry
      // and exit points) to the readout end?
      double x = (ide.entryX + ide.exitX) / 2;
      double y = (ide.entryY + ide.exitY) / 2;
      double z = (ide.entryZ + ide.exitZ) / 2;
      double world[3] = {x, y, z};
      double svHitPosLocal[3];
      double modHitPosLocal[3];
      adsGeo.WorldToLocal(world, svHitPosLocal); //position in strip frame  (origin at center)
      adGeo.WorldToLocal(world, modHitPosLocal); //position in module frame (origin at center)

      if ( abs(svHitPosLocal[0])>adsGeo.HalfWidth1()+0.001 || 
           abs(svHitPosLocal[1])>adsGeo.HalfHeight()+0.001 ||
           abs(svHitPosLocal[2])>adsGeo.HalfLength()+0.001) 
         mf::LogInfo("CRT") << "HIT POINT OUTSIDE OF SENSITIVE VOLUME!" << '\n'
                            << "  AD: " << adid << " , ADS: " << adsid << '\n'
                            << "  Local position (x,y,z): ( " << svHitPosLocal[0]
                            << " , " << svHitPosLocal[1] << " , " << svHitPosLocal[2] << " )" << '\n';

      // The expected number of PE, using a quadratic model for the distance
      // dependence, and scaling linearly with deposited energy.
      double qr = fUseEdep ? ide.energyDeposited / fQ0 : 1.0;
      if (auxDetType == 'c') qr *= 1.5; //c strips 50% thicker

      //longitudinal distance (m) along the strip for fiber atten. calculation
      double distToReadout = abs( adsGeo.HalfLength() - svHitPosLocal[2])*0.01; 
      double distToReadout2 = abs(-adsGeo.HalfLength() - svHitPosLocal[2])*0.01; 

      //coefficients for quadratic fit to MINOS test data w/S14
      //obtained for normally incident cosmic muons
      double p0_m = 36.5425; //initial light yield (pe) before any attenuation in m scintillator
      double p1_m = -6.3895;
      double p2_m =  0.3742;

      double at0_c = 0.682976;
      double at1_c = -0.0204477;
      double at2_c = -0.000707564;
      double at3_c = 0.000636617;
      double at4_c = 0.000147957;
      double at5_c = -3.89078e-05;

      double at0_r = 0.139941;
      double at1_r = 0.168238;
      double at2_r = -0.0198199;
      double at3_r = 0.000781752;

      double at0_l = 8.78875;
      double at1_l = 3.54602;
      double at2_l = 0.595592;
      double at3_l = 0.0449169;
      double at4_l = 0.00127892;

      //scale to LY from normally incident MIP muon  (PE)
      double npeExpected = \
        (p2_m * pow(distToReadout,2) + p1_m * distToReadout + p0_m) * qr;
      double npeExpected2 = \
        (p2_m * pow(distToReadout2,2) + p1_m * distToReadout2 + p0_m) * qr;

      // Put PE on channels weighted by transverse distance across the strip,
      // using an exponential model
      double abs0=0.0, abs1=0.0, arg=0.0; 

      switch(auxDetType){
          case 'c' :
              //hit between both fibers 
              if ( abs(svHitPosLocal[0]) <= 5.5 ) {
                arg=svHitPosLocal[0];
                abs0 = at5_c*pow(arg,5) + at4_c*pow(arg,4) + at3_c*pow(arg,3) \
		  + at2_c*pow(arg,2) + at1_c*arg + at0_c;
                abs1 = -1*at5_c*pow(arg,5) + at4_c*pow(arg,4) - at3_c*pow(arg,3) \
                  + at2_c*pow(arg,2) - at1_c*arg + at0_c;
                break;
              }
              //hit to right of both fibers
	      if ( svHitPosLocal[0] > 5.5 ) {
                arg=svHitPosLocal[0];
		abs0 = at3_r*pow(arg,3) + at2_r*pow(arg,2) + at1_r*arg + at0_r;
		abs1 = at4_l*pow(arg,4) - at3_l*pow(arg,3) \
                  + at2_l*pow(arg,2) - at1_l*arg + at0_l;
                break;
	      }
              //hit to left of both fibers
              if ( svHitPosLocal[0] < -5.5 ) {
		arg=svHitPosLocal[0];
		abs0 = at4_l*pow(arg,4) + at3_l*pow(arg,3) \
                  + at2_l*pow(arg,2) + at1_l*arg + at0_l;
                abs1 = -1*at3_r*pow(arg,3) + at2_r*pow(arg,2) - at1_r*arg + at0_r;
	      }
              break;
          case 'm' : 
              abs0 = 1.0; abs1 = 1.0;
              break;
          case 'd' : 
              abs0 = 1.0; abs1 = 1.0;
              break;
      }

      double npeExp0 = npeExpected * abs0;// / (abs0 + abs1);
      double npeExp1 = npeExpected * abs1;// / (abs0 + abs1);
      double npeExp0Dual = npeExpected2 * abs0;// / (abs0 + abs1);

      if (npeExp0<0||npeExp1<0||npeExp0Dual<0) mf::LogInfo("CRT") << "NEGATIVE PE!!!!!" << '\n';

      // Observed PE (Poisson-fluctuated)
      long npe0 = CLHEP::RandPoisson::shoot(engine, npeExp0);
      long npe1 = CLHEP::RandPoisson::shoot(engine, npeExp1);
      long npe0Dual = CLHEP::RandPoisson::shoot(engine, npeExp0Dual);

      // Time relative to trigger, accounting for propagation delay and 'walk'
      // for the fixed-threshold discriminator
      double tTrue = (ide.entryT + ide.exitT) / 2 + fGlobalT0Offset;
      uint32_t t0 = \
        GetChannelTriggerTicks(engine, trigClock, tTrue, npe0, distToReadout);
      uint32_t t1 = \
        GetChannelTriggerTicks(engine, trigClock, tTrue, npe1, distToReadout);
      uint32_t t0Dual = \
        GetChannelTriggerTicks(engine, trigClock, tTrue, npe0Dual, distToReadout2);

      // Time relative to PPS: Random for now! (FIXME)
      uint32_t ppsTicks = \
        CLHEP::RandFlat::shootInt(engine, trigClock.Frequency() * 1e6);

      // SiPM and ADC response: Npe to ADC counts
      short q0 = \
        CLHEP::RandGauss::shoot(engine, fQPed + fQSlope * npe0, fQRMS * sqrt(npe0));
      short q1 = \
        CLHEP::RandGauss::shoot(engine, fQPed + fQSlope * npe1, fQRMS * sqrt(npe1));
      short q0Dual = \
        CLHEP::RandGauss::shoot(engine, fQPed + fQSlope * npe0Dual, fQRMS * sqrt(npe0Dual));

      if (q0<0||q1<0||q0Dual<0) mf::LogInfo("CRT") << "NEGATIVE ADC!!!!!" << '\n';

      // Adjacent channels on a strip are numbered sequentially.
      //
      // In the AuxDetChannelMapAlg methods, channels are identified by an
      // AuxDet name (retrievable given the hit AuxDet ID) which specifies a
      // module, and a channel number from 0 to 32.

      uint32_t channel0ID=0, channel1ID=0;

      switch (auxDetType){
          case 'c' :
              mac5 = adid;
              channel0ID = 2 * adsid + 0;
              channel1ID = 2 * adsid + 1;
              break;
          case 'd' : 
              mac5 = adid;
              channel0ID = adsid;
              break;
          case 'm' :
              mac5 = adid/3;
              channel0ID = adsid/2 + 10*(adid % 3);
              break;

      }

      if (mac5==UINT32_MAX) mf::LogInfo("CRT") << "mac addrs not set!" << '\n';

      // Apply ADC threshold and strip-level coincidence (both fibers fire)
      if (auxDetType=='c' && q0 > fQThresholdC && q1 > fQThresholdC && util::absDiff(t0, t1) < fStripCoincidenceWindow) {
              Tagger& tagger = taggers[mac5];
              tagger.layerid.insert(layid);
              tagger.chanlayer[channel0ID] = layid;
              tagger.chanlayer[channel1ID] = layid;
              tagger.stackid = stackid;
              tagger.reg = region;
              tagger.type = 'c';
              tagger.data.push_back(icarus::crt::CRTChannelData(channel0ID,t0,ppsTicks,q0));
              tagger.data.push_back(icarus::crt::CRTChannelData(channel1ID,t1,ppsTicks,q1));
              nchandat_c++;
      }//if fiber-fiber coincidence

      if (auxDetType=='d' && q0 > fQThresholdD) {
              Tagger& tagger = taggers[mac5];
              tagger.layerid.insert(layid);
              tagger.chanlayer[channel0ID] = layid;
              tagger.stackid = stackid;
              tagger.reg = region;
              tagger.type = 'd';
              tagger.data.push_back(icarus::crt::CRTChannelData(channel0ID,t0,ppsTicks,q0));
              nchandat_d++;
      }//if one strip above threshold

      if (auxDetType=='m') {
              if(q0 > fQThresholdM) {
                Tagger& tagger = taggers[mac5];
                tagger.layerid.insert(layid);
                tagger.chanlayer[channel0ID] = layid;
                tagger.stackid = stackid;
                tagger.reg = region;
                tagger.type = 'm';
                tagger.data.push_back(icarus::crt::CRTChannelData(channel0ID,t0,ppsTicks,q0));
                nchandat_m++;
              }
              if(q0Dual > fQThresholdM) { 
                Tagger& tagger = taggers[mac5+50];
                tagger.layerid.insert(layid);
                tagger.chanlayer[channel0ID] = layid;
                tagger.stackid = stackid;
                tagger.reg = region;
                tagger.type = 'm';
                tagger.data.push_back(icarus::crt::CRTChannelData(channel0ID,t0Dual,ppsTicks,q0Dual));
                nchandat_m++;
              }
      }//if one strip above threshold at either end

      if (auxDetType == 'c') {
          if (q0 < fQThresholdC || q1 < fQThresholdC) nmissthr_c++;
          if ( util::absDiff(t0,t1) >= fStripCoincidenceWindow ) nmiss_strcoin_c++;
      }
      if (auxDetType == 'd' && q0 < fQThresholdD) nmissthr_d++;
      if (auxDetType == 'm' && ( q0 < fQThresholdM || q0Dual < fQThresholdM)) nmissthr_m++;

      if (fVerbose&&
         ( (auxDetType=='c' && q0>fQThresholdC && q1>fQThresholdC) ||
           (auxDetType=='d' && q0>fQThresholdD ) ||
           (auxDetType=='m' && (q0>fQThresholdM || q0Dual>fQThresholdM)) ))
        mf::LogInfo("CRT")
        << "CRT HIT VOL " << (adGeo.TotalVolume())->GetName() << " with " << adGeo.NSensitiveVolume() << " AuxDetSensitive volumes" << "\n"
        << "CRT HIT SENSITIVE VOL " << (adsGeo.TotalVolume())->GetName() << "\n"
        << "CRT HIT AuxDetID " <<  adsc.AuxDetID() << " / AuxDetSensitiveID " << adsc.AuxDetSensitiveID() << "\n"
        << "CRT module type: " << auxDetType << " , CRT region: " << region << '\n'
        << "CRT channel: " << channel0ID << " , mac5: " << mac5 << '\n'
        << "CRT HIT POS " << x << " " << y << " " << z << "\n"
        << "CRT STRIP POS " << svHitPosLocal[0] << " " << svHitPosLocal[1] << " " << svHitPosLocal[2] << "\n"
        << "CRT MODULE POS " << modHitPosLocal[0] << " " << modHitPosLocal[1] << " "<< modHitPosLocal[2] << " " << "\n"
        << "CRT layer ID: " << layid << "\n"
        << "CRT distToReadout: " << distToReadout << ", distToReadout2: " << distToReadout2 << '\n'
        << "CRT abs0: " << abs0 << " , abs1: " << abs1 << '\n'
        << "CRT npeExpected: " << npeExpected << " , npeExpected2: " << npeExpected2 << '\n'
        << "CRT npeExp0: " << npeExp0 << " , npeExp1: " << npeExp1 << " , npeExp0Dual: " << npeExp0Dual << '\n'
        << "CRT q0: " << q0 << ", q1: " << q1 << ", t0: " << t0 << ", t1: " << t1 << ", dt: " << util::absDiff(t0,t1) << "\n"; 
    }//for AuxDetIDEs 
  }//for AuxDetChannels

  // Apply coincidence trigger requirement
  std::unique_ptr<std::vector<icarus::crt::CRTData> > triggeredCRTHits(
      new std::vector<icarus::crt::CRTData>);

  uint32_t nmiss_lock_c=0, nmiss_lock_d=0, nmiss_lock_m=0;
  uint32_t nmiss_dead_c=0, nmiss_dead_d=0, nmiss_dead_m=0;
  uint32_t nmiss_opencoin_c = 0, nmiss_opencoin_d = 0;
  uint32_t nmiss_coin_c = 0;
  uint32_t nmiss_coin_d = 0;
  uint32_t nmiss_coin_m = 0;
  uint32_t event = 0;
  uint32_t nhit_m=0, nhit_c=0, nhit_d=0;
  uint32_t neve_m=0, neve_c=0, neve_d=0;

  // Loop over all FEBs with a hit and check coincidence requirement.
  // For each FEB, find channel providing trigger and determine if
  //  other hits are in concidence with the trigger (keep) 
  //  or if hits occur during R/O (dead time) (lost)
  //  or if hits are part of a different event (keep for now)
  // First apply dead time correction, biasing effect if configured to do so.
  // Front-end logic: For CERN or DC modules require at least one hit in each X-X layer.
  for (auto trg : taggers) {

      event = 0;
      icarus::crt::CRTChannelData *chanTrigData, *chanTmpData;
      std::set<uint32_t> trackNHold = {};
      std::set<uint32_t> layerNHold = {};
      std::pair<uint32_t,uint32_t> tpair;
      bool minosPairFound = false;
      std::vector<icarus::crt::CRTChannelData> passingData;
      double ttrig=0.0, ttmp=0.0;

      if (trg.second.type=='c' && fApplyCoincidenceC && trg.second.layerid.size()<2) nmiss_opencoin_c++;
      if (trg.second.type=='d' && fApplyCoincidenceD && trg.second.layerid.size()<2) nmiss_opencoin_d++;

      //for C and D modules, check if coincidence possible, if enabled
      if (trg.second.type=='m' || 
          (trg.second.type=='c' && 
             (!fApplyCoincidenceC||(fApplyCoincidenceC && trg.second.layerid.size()>1))) ||
          (trg.second.type=='d' && 
             (!fApplyCoincidenceD||(fApplyCoincidenceD && trg.second.layerid.size()>1))) ) {

        //time order ChannalData objects by T0
        std::sort((trg.second.data).begin(),(trg.second.data).end(),TimeOrderCRTData);
 
        //get data for earliest entry
        chanTrigData = &(trg.second.data[0]);
        ttrig = trigClock.Time((double)chanTrigData->T0()); //in us
        trackNHold.insert(chanTrigData->Channel());
        layerNHold.insert(trg.second.chanlayer[chanTrigData->Channel()]);
        passingData.push_back(*chanTrigData);

        //loop over all data products for this FEB
        for ( size_t i=1; i< trg.second.data.size(); i++ ) {

          chanTmpData = &(trg.second.data[i]);
          ttmp = trigClock.Time((double)chanTmpData->T0()); //in us

          //check that time sorting works
          if ( ttmp < ttrig ) mf::LogInfo("CRT") << "SORTING OF DATA PRODUCTS FAILED!!!"<< "\n";

          //for C and D modules only and coin. enabled, if assumed trigger channel has no coincidence
          // set trigger channel to tmp channel and try again
          if ( layerNHold.size()==1 &&
               ( (trg.second.type=='c' && fApplyCoincidenceC && ttmp - ttrig > fLayerCoincidenceWindowC*1e-3) ||
                 (trg.second.type=='d' && fApplyCoincidenceD && ttmp - ttrig > fLayerCoincidenceWindowD*1e-3)) ) {
             chanTrigData = chanTmpData;
             ttrig = ttmp;
             trackNHold.clear();
             layerNHold.clear();
             passingData.clear();
             trackNHold.insert(chanTrigData->Channel());
             layerNHold.insert(trg.second.chanlayer[chanTrigData->Channel()]);
             passingData.push_back(*chanTrigData);
             if(trg.second.type=='c') nmiss_coin_c++;
             if(trg.second.type=='d') nmiss_coin_d++;
             continue;
          }

          //check if coincidence condtion met
          //for c and d modules, just need time stamps within tagger obj
          //for m modules, need to check coincidence with other tagger objs
          if (trg.second.type=='m' && !minosPairFound && fApplyCoincidenceM) {
             for (auto trg2 : taggers) {
               if( trg2.second.type=='m' && 
                   trg.first != trg2.first &&
                   trg.second.stackid == trg2.second.stackid &&
                   trg.second.reg == trg2.second.reg &&
                   ((trg2.second.layerid.find(1) != trg2.second.layerid.end() &&
                    trg.second.layerid.find(0) != trg.second.layerid.end()) ||
                   (trg2.second.layerid.find(0) != trg2.second.layerid.end() && 
                    trg.second.layerid.find(1) != trg.second.layerid.end())) ) {

                    std::sort((trg2.second.data).begin(),(trg2.second.data).end(),TimeOrderCRTData);

                    for ( size_t j=0; j< trg2.second.data.size(); j++ ) {
                       double t2tmp = trigClock.Time((double)trg2.second.data[j].T0()); //in us
		       if ( trg.first != trg2.first && util::absDiff(t2tmp,ttrig) < fLayerCoincidenceWindowM*1e-3) {
                           minosPairFound = true;
                           trg.second.macPair = std::make_pair(trg.first,trg2.first);
                           break;
                       }
                    }
                    if (minosPairFound) break;
                }//if opposite layers in a stack
              }//loop over febs
              if(!minosPairFound) {
                  chanTrigData = chanTmpData;
                  ttrig = ttmp;
                  trackNHold.clear();
                  layerNHold.clear();
                  passingData.clear();
                  trackNHold.insert(chanTrigData->Channel());
                  layerNHold.insert(trg.second.chanlayer[chanTrigData->Channel()]);
                  passingData.push_back(*chanTrigData);
                  nmiss_coin_m++;
                  continue;
              }
          }//if minos module and no pair yet found
          if (trg.second.type != 'm') trg.second.macPair = std::make_pair(trg.first,trg.first);

          uint32_t adctmp = 0;
          //currently assuming bias time is same as track and hold window (FIX ME!)
          if ( (trg.second.type=='c' && ttmp < ttrig + fLayerCoincidenceWindowC*1e-3) || 
               (trg.second.type=='d' && ttmp < ttrig + fLayerCoincidenceWindowD*1e-3) ||
               (trg.second.type=='m' && ttmp < ttrig + fLayerCoincidenceWindowM*1e-3)) {
               if ((trackNHold.insert(chanTmpData->Channel())).second) {
                 passingData.push_back(*chanTmpData);
                 if (layerNHold.insert(trg.second.chanlayer[chanTmpData->Channel()]).second)
                   tpair=std::make_pair(chanTrigData->Channel(),chanTmpData->Channel());
               }
               else if (ttmp < ttrig + fBiasTime) {
                 adctmp = (passingData.back()).ADC();
                 adctmp += chanTmpData->ADC();
                 (passingData.back()).SetADC(adctmp);
               }
               else switch (trg.second.type) {
                   case 'c' : nmiss_lock_c++; break;
                   case 'd' : nmiss_lock_d++; break;
                   case 'm' : nmiss_lock_m++; break;
               }
          }//if hits inside readout window
          else if ( ttmp <= ttrig + fDeadTime ) {
              switch (trg.second.type) {
                  case 'c' : nmiss_dead_c++; break;
                  case 'd' : nmiss_dead_d++; break;
                  case 'm' : nmiss_dead_m++; break;
              }
              continue;
          }
          //"read out" data for this event, first hit after dead time as next trigger channel
          else if ( ttmp > ttrig + fDeadTime ) {
            uint32_t regnum = GetAuxDetRegionNum(trg.second.reg);
            if( (regions.insert(regnum)).second) regCounts[regnum] = 1;
            else regCounts[regnum]++;

            triggeredCRTHits->push_back(
              icarus::crt::CRTData(trg.first,event,ttrig,chanTrigData->Channel(),tpair,trg.second.macPair,passingData));
            event++;
            if (trg.second.type=='c') {neve_c++; nhit_c+=passingData.size(); }
            if (trg.second.type=='d') {neve_d++; nhit_d+=passingData.size(); }
            if (trg.second.type=='m') {neve_m++; nhit_m+=passingData.size(); }
            ttrig = ttmp;
            chanTrigData = chanTmpData;
            passingData.clear();
            trackNHold.clear();
            layerNHold.clear();
            passingData.push_back(*chanTrigData);
            trackNHold.insert(chanTrigData->Channel());
            layerNHold.insert(trg.second.chanlayer[chanTmpData->Channel()]);
            minosPairFound = false;
          }

        }//for data entries (hits)
    } //if intermodule coincidence or minos module
  } // for taggers

  if (fVerbose) { 
    mf::LogInfo("CRT") << "CRT TRIGGERED HITS: " << triggeredCRTHits->size() << "\n"
     << "CERN sim hits: " << nsim_c << '\n'
     << "DC sim hits: " << nsim_d << '\n'
     << "MINOS sim hits: " << nsim_m << '\n'
     << "CERN hits > thresh: " << nchandat_c << '\n'
     << "DC hits > thresh: " << nchandat_d << '\n'
     << "MINOS hits > thresh: " << nchandat_m << '\n'
     << "CERN hits lost from threshold: " << nmissthr_c << '\n'
     << "CERN hits lost from fiber coincidence: " << nmiss_strcoin_c << '\n'
     << "DC hits lost from threshold: " << nmissthr_d << '\n'
     << "MINOS hits lost from threshold: " << nmissthr_m << '\n'
     << "CERN hits lost from open coincidence: " << nmiss_opencoin_c << '\n'
     << "DC hits lost from open coincidence: " << nmiss_opencoin_d << '\n'
     << "CERN missed hits from trackNHold: " <<nmiss_lock_c << " (" << 100.0*nmiss_lock_c/nchandat_c << "%)" << '\n' 
     << "DC missed hits from trackNHold: " <<nmiss_lock_d << " (" << 100.0*nmiss_lock_d/nchandat_d << "%)" <<'\n'
     << "MINOS missed hits from trackNHold: " <<nmiss_lock_m << " (" << 100.0*nmiss_lock_m/nchandat_m << "%)" << '\n'
     << "CERN missed hits from deadTime: " << nmiss_dead_c << " (" << 100.0*nmiss_dead_c/nchandat_c << "%)" << '\n'
     << "DC missed hits from deadTime: " << nmiss_dead_d << " (" << 100.0*nmiss_dead_d/nchandat_d << "%)" << '\n'
     << "MINOS missed hits from deadTime: " << nmiss_dead_m << " (" << 100.0*nmiss_dead_m/nchandat_m << "%)" << '\n'
     << "missed CERN  hits from coincidence: " << nmiss_coin_c << " (" << 100.0*nmiss_coin_c/nchandat_c << "%)" << '\n'
     << "missed DC    hits from coincidence: " << nmiss_coin_d << " (" << 100.0*nmiss_coin_d/nchandat_d << "%)" << '\n'
     << "missed MINOS hits from coincidence: " << nmiss_coin_m << " (" << 100.0*nmiss_coin_m/nchandat_m << "%)" << '\n'
     << "hits in CERN system: " << nhit_c << " (" << 100.0*nhit_c/nchandat_c << "%)" << '\n'
     << "hits in DC system: " << nhit_d << " (" << 100.0*nhit_d/nchandat_d << "%)" << '\n'
     << "hits in MINOS system: " << nhit_m << " (" << 100.0*nhit_m/nchandat_m << "%)" << '\n'
     << "events in CERN system: " << neve_c << '\n'
     << "events in DC system: " << neve_d << '\n'
     << "events in MINOS system: " << neve_m << '\n';
     
    std::map<uint32_t,uint32_t>::iterator it = regCounts.begin();
    mf::LogInfo("CRT") << '\n' << "FEB events per CRT region: " << '\n';
     
    while ( it != regCounts.end() ) {
        mf::LogInfo("CRT") << "reg: " << (*it).first << " , events: " << (*it).second << '\n';
        it++;
    }
  } //if verbose

  e.put(std::move(triggeredCRTHits));
}

DEFINE_ART_MODULE(CRTDetSim)

}  // namespace crt
}  // namespace icarus

