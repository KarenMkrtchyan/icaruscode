////////////////////////////////////////////////////////////////////////
// Class:       PMTLaserCalibration
// Plugin Type: analyzer (art v3_05_00)
// File:        PMTLaserCalibration_module.cc
//
// Generated at Mon Sep 21 15:21:37 2020 by Andrea Scarpelli
// 
//  Data prep for both charge and time calibration using laser pulses
//
//  mailto:ascarpell@bnl.gov
////////////////////////////////////////////////////////////////////////

#include "larcore/Geometry/Geometry.h"
#include "larcore/CoreUtils/ServiceUtil.h" 

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "canvas/Utilities/Exception.h"

#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"


#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art_root_io/TFileService.h"
#include "lardataobj/RawData/OpDetWaveform.h"

#include "icaruscode/Decode/ChannelMapping/IICARUSChannelMap.h"
#include "icaruscode/PMT/Calibration/CaloTools/LaserPulse.h"


#include "TTree.h"

namespace pmtcalo {
  class PMTLaserCalibration;
}


class pmtcalo::PMTLaserCalibration : public art::EDAnalyzer {

public:

  explicit PMTLaserCalibration(fhicl::ParameterSet const& pset);

  PMTLaserCalibration(PMTLaserCalibration const&) = delete;
  PMTLaserCalibration(PMTLaserCalibration&&) = delete;
  PMTLaserCalibration& operator=(PMTLaserCalibration const&) = delete;
  PMTLaserCalibration& operator=(PMTLaserCalibration&&) = delete;

  virtual void beginJob() override;

  void analyze(art::Event const& event) override;

  void clean();

private:

  art::InputTag m_data_label;

  bool m_filter_noise;
  fhicl::ParameterSet m_waveform_config;

  int m_run;
  int m_event;

  TTree *m_pulse_ttree;

  const icarusDB::IICARUSChannelMap* fChannelMap;

  std::vector<float> *m_channel_id = NULL;
  std::vector<float> *m_baseline = NULL;
  std::vector<float> *m_rms = NULL;
  std::vector<float> *m_peak_time = NULL;
  std::vector<float> *m_amplitude = NULL;
  std::vector<float> *m_integral = NULL;
  std::vector<float> *m_total_charge = NULL;

  // fitted quantities
  std::vector<float> *m_fit_start_time = NULL;
  std::vector<float> *m_error_start_time = NULL;
  std::vector<float> *m_fit_sigma = NULL;
  std::vector<float> *m_error_sigma = NULL;
  std::vector<float> *m_fit_mu = NULL;
  std::vector<float> *m_error_mu = NULL;
  std::vector<float> *m_fit_amplitude = NULL;
  std::vector<float> *m_error_amplitude = NULL;
  std::vector<float> *m_chi2 = NULL;
  std::vector<float> *m_ndf = NULL;
  std::vector<float> *m_fitstatus = NULL; // O:good, >0: bad,  < 0: not working

  art::ServiceHandle<art::TFileService> tfs;

  LaserPulse *myWaveformAna;

};


//------------------------------------------------------------------------------


pmtcalo::PMTLaserCalibration::PMTLaserCalibration(fhicl::ParameterSet const& pset)
  : art::EDAnalyzer(pset)  // ,
{ 

   m_data_label = pset.get<art::InputTag>("InputModule", "daqPMT");
   m_filter_noise = pset.get<bool>("FilterNoise", false);
   m_waveform_config = pset.get<fhicl::ParameterSet>("WaveformAnalysis");


   myWaveformAna = new LaserPulse(m_waveform_config);

   // Configure the channel mapping services
   fChannelMap = art::ServiceHandle<icarusDB::IICARUSChannelMap const>{}.get();
}


//------------------------------------------------------------------------------


void pmtcalo::PMTLaserCalibration::beginJob()
{

  //For direct light calibration and timing
  m_pulse_ttree = tfs->make<TTree>("pulsetree","tree with laser pulse characterization");

  m_pulse_ttree->Branch("run", &m_run, "run/I" );
  //m_pulse_ttree->Branch("subrun", &m_subrun, "subrun/I" );
  m_pulse_ttree->Branch("event", &m_event, "event/I" );
  m_pulse_ttree->Branch("channel_id", &m_channel_id );
  m_pulse_ttree->Branch("baseline", &m_baseline );
  m_pulse_ttree->Branch("rms", &m_rms );
  m_pulse_ttree->Branch("peak_time", &m_peak_time );
  m_pulse_ttree->Branch("amplitude", &m_amplitude );
  m_pulse_ttree->Branch("integral", &m_integral );
  m_pulse_ttree->Branch("total_charge", &m_total_charge );

  m_pulse_ttree->Branch("fit_start_time", &m_fit_start_time );
  m_pulse_ttree->Branch("error_start_time", &m_error_start_time );
  m_pulse_ttree->Branch("fit_sigma", &m_fit_sigma);
  m_pulse_ttree->Branch("error_sigma", &m_error_sigma);
  m_pulse_ttree->Branch("fit_mu", &m_fit_mu);
  m_pulse_ttree->Branch("error_mu", &m_error_mu);
  m_pulse_ttree->Branch("fit_amplitude", &m_fit_amplitude);
  m_pulse_ttree->Branch("error_amplitude", &m_error_amplitude);
  m_pulse_ttree->Branch("chi2", &m_chi2);
  m_pulse_ttree->Branch("ndf", &m_ndf);
  m_pulse_ttree->Branch("fitstatus", &m_fitstatus);

}


//-----------------------------------------------------------------------------


void pmtcalo::PMTLaserCalibration::analyze(art::Event const& event)
{ 

   m_run = event.id().run();
   m_event = event.id().event();
  
   art::Handle< std::vector< raw::OpDetWaveform > > rawHandle;
   event.getByLabel(m_data_label, rawHandle);


   // There is a valid handle per channel
   for( auto const& raw_waveform : (*rawHandle) )
   {

     raw::Channel_t channel_id = raw_waveform.ChannelNumber();

     m_channel_id->push_back( channel_id );
     
     myWaveformAna->loadData( raw_waveform );
     if( m_filter_noise ){ myWaveformAna->filterNoise(); }

     auto pulse = myWaveformAna->getLaserPulse();

     // Mostly here we fill up our TTrees

     m_peak_time->push_back( pulse.time_peak );
     m_amplitude->push_back( pulse.amplitude );
     m_integral->push_back( pulse.integral );
     m_total_charge->push_back( myWaveformAna->getTotalCharge() );

     m_fit_start_time->push_back(pulse.fit_start_time);
     m_error_start_time->push_back(pulse.error_start_time);
     m_fit_sigma->push_back(pulse.fit_sigma);
     m_error_sigma->push_back(pulse.error_sigma);
     m_fit_mu->push_back(pulse.fit_mu);
     m_error_mu->push_back(pulse.error_mu);
     m_fit_amplitude->push_back(pulse.fit_amplitude);
     m_error_amplitude->push_back(pulse.error_amplitude);
     m_chi2->push_back(pulse.chi2);
     m_ndf->push_back(pulse.ndf);
     m_fitstatus->push_back(pulse.fitstatus);

     // Prepare for the next event
     myWaveformAna->clean();

   } // end loop over pmt channels

   m_pulse_ttree->Fill();

   // Cancel the arrays
   clean();


} // end analyze


//-----------------------------------------------------------------------------

void pmtcalo::PMTLaserCalibration::clean(){

  m_channel_id->clear();
  m_peak_time->clear();
  m_amplitude->clear();
  m_integral->clear();
  m_total_charge->clear();

  m_fit_start_time->clear();
  m_error_start_time->clear();
  m_fit_sigma->clear();
  m_error_sigma->clear();
  m_fit_mu->clear();
  m_error_mu->clear();
  m_fit_amplitude->clear();
  m_error_amplitude->clear();
  m_chi2->clear();
  m_ndf->clear();
  m_fitstatus->clear();

}


DEFINE_ART_MODULE(pmtcalo::PMTLaserCalibration)
