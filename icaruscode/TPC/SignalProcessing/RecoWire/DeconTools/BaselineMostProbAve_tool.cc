////////////////////////////////////////////////////////////////////////
/// \file   Baseline.cc
/// \author T. Usher
////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "icaruscode/TPC/SignalProcessing/RecoWire/DeconTools/IBaseline.h"
#include "art/Utilities/ToolMacros.h"
#include "art/Utilities/make_tool.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/TFileService.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "cetlib_except/exception.h"
#include "larcore/CoreUtils/ServiceUtil.h" // lar::providerFrom()
#include "icaruscode/TPC/Utilities/SignalShapingICARUSService_service.h"
#include "icarus_signal_processing/WaveformTools.h"

#include <fstream>
#include <algorithm> // std::minmax_element()

namespace icarus_tool
{

class BaselineMostProbAve : IBaseline
{
public:
    explicit BaselineMostProbAve(const fhicl::ParameterSet& pset);
    
    ~BaselineMostProbAve();
    
    void configure(const fhicl::ParameterSet& pset)                                        override;
    void outputHistograms(art::TFileDirectory&)                                      const override;
    
    float GetBaseline(const std::vector<float>&, raw::ChannelID_t, size_t, size_t)   const override;
    
private:
    std::pair<float,int> GetBaseline(const std::vector<float>&, int, size_t, size_t) const;
    
    size_t fMaxROILength;    ///< Maximum length for calculating Most Probable Value

    icarus_signal_processing::WaveformTools<double>                       fWaveformTool;

    art::ServiceHandle<icarusutil::SignalShapingICARUSService> fSignalShaping;
};
    
//----------------------------------------------------------------------
// Constructor.
BaselineMostProbAve::BaselineMostProbAve(const fhicl::ParameterSet& pset)
{
    configure(pset);
}
    
BaselineMostProbAve::~BaselineMostProbAve()
{
}
    
void BaselineMostProbAve::configure(const fhicl::ParameterSet& pset)
{
    // Recover our fhicl variable
    fMaxROILength = pset.get<size_t>("MaxROILength", 100);
    
    // Get signal shaping service.
    fSignalShaping = art::ServiceHandle<icarusutil::SignalShapingICARUSService>();

    return;
}

    
float BaselineMostProbAve::GetBaseline(const std::vector<float>& holder,
                                       raw::ChannelID_t          channel,
                                       size_t                    roiStart,
                                       size_t                    roiLen) const
{
    float base(0.);

    if (roiLen > 1)
    {
        // Recover the expected electronics noise on this channel
        float  deconNoise = 1.26491 * fSignalShaping->GetDeconNoise(channel);
        int    binRange   = std::max(1, int(std::round(deconNoise)));
        size_t halfLen    = std::min(fMaxROILength,roiLen/2);
        size_t roiStop    = roiStart + roiLen;
        
        // This returns back the mean value and the spread from which it was calculated
        std::pair<float,int> baseFront = GetBaseline(holder, binRange, roiStart,          roiStart + halfLen);
        std::pair<float,int> baseBack  = GetBaseline(holder, binRange, roiStop - halfLen, roiStop           );

//        std::cout << "-Baseline size: " << holder.size() << ", front/back: " << baseFront.first << "/" << baseBack.first << ", ranges: " << baseFront.second << "/" << baseBack.second;
        
        // Check for a large spread between the two estimates
        if (std::abs(baseFront.first - baseBack.first) > 1.5 * deconNoise)
        {
            // We're going to favor the front, generally, unless the spread on the back is lower
            if (baseFront.second < 3 * baseBack.second / 2) base = baseFront.first;
            else                                            base = baseBack.first;
        }
        else
        {
            // Otherwise we take a weighted average between the two 
            float rangeFront = baseFront.second;
            float rangeBack  = baseBack.second;

            base = (baseFront.first/rangeFront + baseBack.first/rangeBack)*(rangeFront*rangeBack)/(rangeFront+rangeBack);
        }

//        std::cout << ", base: " << base << std::endl;
    }
    
    return base;
}
    
std::pair<float,int> BaselineMostProbAve::GetBaseline(const std::vector<float>& holder,
                                                      int                       binRange,
                                                      size_t                    roiStart,
                                                      size_t                    roiStop) const
{
    std::pair<double,int> base(0.,1);
    int                   nTrunc;
    
    if (roiStop > roiStart)
    {
        // Get the truncated mean and rms
        icarusutil::TimeVec temp(roiStop - roiStart + 1,0.);
        
        std::copy(holder.begin() + roiStart,holder.begin() + roiStop,temp.begin());
        
        fWaveformTool.getTruncatedMean(temp, base.first, nTrunc, base.second);
    }
    
    return base;
}
    
void BaselineMostProbAve::outputHistograms(art::TFileDirectory& histDir) const
{
    // It is assumed that the input TFileDirectory has been set up to group histograms into a common
    // folder at the calling routine's level. Here we create one more level of indirection to keep
    // histograms made by this tool separate.
/*
    std::string dirName = "BaselinePlane_" + std::to_string(fPlane);
    
    art::TFileDirectory dir = histDir.mkdir(dirName.c_str());
    
    auto const* detprop      = lar::providerFrom<detinfo::DetectorPropertiesService>();
    double      samplingRate = detprop->SamplingRate();
    double      numBins      = fBaselineVec.size();
    double      maxFreq      = 500. / samplingRate;
    std::string histName     = "BaselinePlane_" + std::to_string(fPlane);
    
    TH1D*       hist         = dir.make<TH1D>(histName.c_str(), "Baseline;Frequency(MHz)", numBins, 0., maxFreq);
    
    for(int bin = 0; bin < numBins; bin++)
    {
        double freq = maxFreq * double(bin + 0.5) / double(numBins);
        
        hist->Fill(freq, fBaselineVec.at(bin).Re());
    }
*/
    
    return;
}
    
DEFINE_ART_CLASS_TOOL(BaselineMostProbAve)
}
