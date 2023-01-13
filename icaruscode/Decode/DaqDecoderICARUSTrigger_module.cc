///////////////////////////////////////////////////////
///// Class: DaqDecoderICARUSTrigger
///// Plugin Type: producer
///// File: DaqDecoderICARUSTrigger.cc
//////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Utilities/make_tool.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "artdaq-core/Data/Fragment.hh"

#include "icaruscode/Decode/DecoderTools/IDecoder.h"

#include <iostream>
#include <cstdlib>
#include <vector>

namespace daq 
{
  class DaqDecoderICARUSTrigger: public art::EDProducer
  {
  public:
    explicit DaqDecoderICARUSTrigger(fhicl::ParameterSet const & p);
    DaqDecoderICARUSTrigger(DaqDecoderICARUSTrigger const &) = delete;
    DaqDecoderICARUSTrigger(DaqDecoderICARUSTrigger &&) = delete;
    DaqDecoderICARUSTrigger & operator = (DaqDecoderICARUSTrigger const &) = delete;
    DaqDecoderICARUSTrigger & operator = (DaqDecoderICARUSTrigger &&) = delete;

    void beginRun(art::Run& run) override;
    void produce(art::Event & e) override;
    
  private:
    std::unique_ptr<IDecoder> fDecoderTool;
    art::InputTag fInputTag;

  };

  DEFINE_ART_MODULE(DaqDecoderICARUSTrigger)
  
  DaqDecoderICARUSTrigger::DaqDecoderICARUSTrigger(fhicl::ParameterSet const & params): art::EDProducer{params}, fInputTag(params.get<std::string>("FragmentsLabel", "daq:ICARUSTriggerUDP"))
  {
    if (!fInputTag.empty()) mayConsume<artdaq::Fragments>(fInputTag);
    
    fDecoderTool = art::make_tool<IDecoder>(params.get<fhicl::ParameterSet>("DecoderTool"));
    fDecoderTool->consumes(consumesCollector());
    fDecoderTool->produces(producesCollector());
    
    return;
  }
  
  void DaqDecoderICARUSTrigger::beginRun(art::Run& run)
  {
    fDecoderTool->setupRun(run);
  }
  
  
  void DaqDecoderICARUSTrigger::produce(art::Event & event)
  {
    fDecoderTool->initializeDataProducts();
    
    // if the tool asks for a specific input, use that one; otherwise, try the configured one.
    art::InputTag const& inputTag = fDecoderTool->preferredInput().value_or(fInputTag);
    
    auto const & daq_handle = event.getValidHandle<artdaq::Fragments>(inputTag);
    if(daq_handle.isValid() && daq_handle->size() > 0)
    {
      for (auto const & rawFrag: *daq_handle) fDecoderTool->process_fragment(rawFrag);
    }
    else
      std::cout << "No Trigger Fragment Information Found!" << std::endl;

    fDecoderTool->outputDataProducts(event);
    return;

  }
      
} //end namespace
