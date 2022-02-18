// icaruscode includes
#include "sbnobj/ICARUS/CRT/CRTData.hh"
#include "sbnobj/Common/CRT/CRTHit.hh"
#include "icaruscode/CRT/CRTUtils/CRTHitRecoAlg.h"
#include "icaruscode/Decode/DataProducts/ExtraTriggerInfo.h"

// Framework includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Event.h" 
#include "canvas/Persistency/Common/Ptr.h" 
#include "canvas/Persistency/Common/PtrVector.h" 
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileService.h" 
#include "art_root_io/TFileDirectory.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "art/Persistency/Common/PtrMaker.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "canvas/Utilities/Exception.h"

// C++ includes
#include <memory>
#include <iostream>
#include <map>
#include <iterator>
#include <algorithm>
#include <vector>

// LArSoft
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/AuxDetSimChannel.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/AuxDetGeometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardataobj/RawData/ExternalTrigger.h"
#include "larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

// ROOT
#include "TTree.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TVector3.h"
#include "TGeoManager.h"

using std::vector;

namespace icarus {
namespace crt {

  class CRTSimHitProducer : public art::EDProducer {
  public:
 
    using CRTHit = sbn::crt::CRTHit;
    
    explicit CRTSimHitProducer(fhicl::ParameterSet const & p);

    // The destructor generated by the compiler is fine for classes
    // without bare pointers or other resource use.

    // Plugins should not be copied or assigned.
    CRTSimHitProducer(CRTSimHitProducer const &) = delete;
    CRTSimHitProducer(CRTSimHitProducer &&) = delete;
    CRTSimHitProducer & operator = (CRTSimHitProducer const &) = delete; 
    CRTSimHitProducer & operator = (CRTSimHitProducer &&) = delete;

    // Required functions.
    void produce(art::Event & e) override;

    // Selected optional functions.
    void beginJob() override;

    void endJob() override;

    void reconfigure(fhicl::ParameterSet const & p);

  private:

    // Params from fcl file.......
    art::InputTag fCrtModuleLabel;      ///< name of crt producer
    art::InputTag fTriggerLabel;        ///< name of trigger producer
    CRTHitRecoAlg hitAlg;

    uint64_t m_trigger_timestamp;

  }; // class CRTSimHitProducer

  CRTSimHitProducer::CRTSimHitProducer(fhicl::ParameterSet const & p)
  : EDProducer{p}, hitAlg(p.get<fhicl::ParameterSet>("HitAlg"))
  // Initialize member data here, if know don't want to reconfigure on the fly
  {
 
   // Call appropriate produces<>() functions here.
    produces< vector<CRTHit> >();
    produces< art::Assns<CRTHit, CRTData> >();
    
    reconfigure(p);

  } // CRTSimHitProducer()

  void CRTSimHitProducer::reconfigure(fhicl::ParameterSet const & p)
  {
    fCrtModuleLabel = (p.get<art::InputTag> ("CrtModuleLabel")); 
    fTriggerLabel   = (p.get<art::InputTag> ("TriggerLabel")); 
  } // CRTSimHitProducer::reconfigure()

  void CRTSimHitProducer::beginJob()
  {

  } // CRTSimHitProducer::beginJob()
 
  void CRTSimHitProducer::produce(art::Event & event)
  {

    std::unique_ptr< vector<CRTHit> > CRTHitcol( new vector<CRTHit>);
    std::unique_ptr< art::Assns<CRTHit, CRTData> > Hitassn( new art::Assns<CRTHit, CRTData>);
    art::PtrMaker<sbn::crt::CRTHit> makeHitPtr(event);

    int nHits = 0;

    // Retrieve list of CRT hits
    art::Handle< std::vector<CRTData>> crtListHandle;
    vector<art::Ptr<CRTData> > crtList;

    if (event.getByLabel(fCrtModuleLabel, crtListHandle))
      art::fill_ptr_vector(crtList, crtListHandle);

    //add trigger info
    if( !fTriggerLabel.empty() ) {

      art::Handle<sbn::ExtraTriggerInfo> trigger_handle;
      event.getByLabel( fTriggerLabel, trigger_handle );
      if( trigger_handle.isValid() )
	m_trigger_timestamp = trigger_handle->triggerTimestamp; 
      else
	mf::LogError("CRTSimHitProducer") << "No raw::Trigger associated to label: " << fTriggerLabel.label() << "\n" ;
    } else{ 
      std::cout  << "Trigger Data product " << fTriggerLabel.label() << " not found!\n" ;
    }

    mf::LogInfo("CRTSimHitProducer")
      <<"Number of SiPM hits = "<<crtList.size();

    vector<art::Ptr<CRTData>> crtData = hitAlg.PreselectCRTData(crtList, m_trigger_timestamp);

    vector<std::pair<CRTHit, vector<int>>> crtHitPairs = hitAlg.CreateCRTHits(crtData);
    //vector<std::pair<CRTHit, vector<int>>> crtHitPairs = hitAlg.CreateCRTHits(crtList);

    mf::LogInfo("CRTSimHitProducer")
      << "Number of CRTHit,data indices pairs = " << crtHitPairs.size();

    for(auto const& crtHitPair : crtHitPairs){

      CRTHitcol->push_back(crtHitPair.first);
      art::Ptr<CRTHit> hitPtr = makeHitPtr(CRTHitcol->size()-1);
      nHits++;

      for(auto const& data_i : crtHitPair.second){

        Hitassn->addSingle(hitPtr, crtList[data_i]);
      }
    }
      
    event.put(std::move(CRTHitcol));
    event.put(std::move(Hitassn));

    mf::LogInfo("CRTSimHitProducer")
      <<"Number of CRT hits produced = "<<nHits;

  } // CRTSimHitProducer::produce()

  void CRTSimHitProducer::endJob()
  {

  } // CRTSimHitProducer::endJob()

  DEFINE_ART_MODULE(CRTSimHitProducer)

}
} //end namespace
