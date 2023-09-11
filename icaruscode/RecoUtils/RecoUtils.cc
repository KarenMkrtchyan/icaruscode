#include "RecoUtils.h"

int RecoUtils::TrueParticleID(const detinfo::DetectorClocksData& clockData,
                              const art::Ptr<recob::Hit>& hit) {
  double particleEnergy = 0;
  int likelyTrackID = 0;
  art::ServiceHandle<cheat::BackTrackerService> bt;
  std::vector<sim::TrackIDE> trackIDs = bt->HitToTrackIDEs(clockData, hit);
  for (unsigned int idIt = 0; idIt < trackIDs.size(); ++idIt) {
    if (trackIDs.at(idIt).energy > particleEnergy) {
      particleEnergy = trackIDs.at(idIt).energy;
      likelyTrackID = trackIDs.at(idIt).trackID;
    }
  }
  return likelyTrackID;
}


int RecoUtils::TrueParticleIDFromTotalTrueEnergy(const detinfo::DetectorClocksData& clockData,
                                                 const std::vector<art::Ptr<recob::Hit> >& hits) {
  art::ServiceHandle<cheat::BackTrackerService> bt;
  std::map<int,double> trackIDToEDepMap;
  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitIt = hits.begin(); hitIt != hits.end(); ++hitIt) {
    art::Ptr<recob::Hit> hit = *hitIt;
    std::vector<sim::TrackIDE> trackIDs = bt->HitToTrackIDEs(clockData, hit);
    for (unsigned int idIt = 0; idIt < trackIDs.size(); ++idIt) {
      trackIDToEDepMap[trackIDs[idIt].trackID] += trackIDs[idIt].energy;
    }
  }

  //Loop over the map and find the track which contributes the highest energy to the hit vector
  double maxenergy = -1;
  int objectTrack = -99999;
  for (std::map<int,double>::iterator mapIt = trackIDToEDepMap.begin(); mapIt != trackIDToEDepMap.end(); mapIt++){
    double energy = mapIt->second;
    double trackid = mapIt->first;
    if (energy > maxenergy){
      maxenergy = energy;
      objectTrack = trackid;
    }
  }

  return objectTrack;
}



int RecoUtils::TrueParticleIDFromTotalRecoCharge(const detinfo::DetectorClocksData& clockData,
                                                 const std::vector<art::Ptr<recob::Hit> >& hits) {
  // Make a map of the tracks which are associated with this object and the charge each contributes
  std::map<int,double> trackMap;
  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitIt = hits.begin(); hitIt != hits.end(); ++hitIt) {
    art::Ptr<recob::Hit> hit = *hitIt;
    int trackID = TrueParticleID(clockData, hit);
    trackMap[trackID] += hit->Integral();
  }

  // Pick the track with the highest charge as the 'true track'
  double highestCharge = 0;
  int objectTrack = -99999;
  for (std::map<int,double>::iterator trackIt = trackMap.begin(); trackIt != trackMap.end(); ++trackIt) {
    if (trackIt->second > highestCharge) {
      highestCharge = trackIt->second;
      objectTrack  = trackIt->first;
    }
  }
  return objectTrack;
}



int RecoUtils::TrueParticleIDFromTotalRecoHits(const detinfo::DetectorClocksData& clockData,
                                               const std::vector<art::Ptr<recob::Hit> >& hits) {
  // Make a map of the tracks which are associated with this object and the number of hits they are the primary contributor to
  std::map<int,int> trackMap;
  for (std::vector<art::Ptr<recob::Hit> >::const_iterator hitIt = hits.begin(); hitIt != hits.end(); ++hitIt) {
    art::Ptr<recob::Hit> hit = *hitIt;
    int trackID = TrueParticleID(clockData, hit);
    trackMap[trackID]++;
  }

  // Pick the track which is the primary contributor to the most hits as the 'true track'
  int objectTrack = -99999;
  int highestCount = -1;
  for (std::map<int,int>::iterator trackIt = trackMap.begin(); trackIt != trackMap.end(); ++trackIt) {
    if (trackIt->second > highestCount) {
      highestCount = trackIt->second;
      objectTrack  = trackIt->first;
    }
  }
  return objectTrack;
}



bool RecoUtils::IsInsideTPC(TVector3 position, double distance_buffer){
  bool inside = false;
  art::ServiceHandle<geo::Geometry> geom;
  geo::TPCID idtpc = geom->FindTPCAtPosition(geo::vect::toPoint(position));

  if (geom->HasTPC(idtpc))
  {
    const geo::TPCGeo& tpcgeo = geom->GetElement(idtpc);
    double minx = tpcgeo.MinX(); double maxx = tpcgeo.MaxX();
    double miny = tpcgeo.MinY(); double maxy = tpcgeo.MaxY();
    double minz = tpcgeo.MinZ(); double maxz = tpcgeo.MaxZ();

    for (auto const& tpcg : geom->Iterate<geo::TPCGeo>())
    {
        if (tpcg.MinX() < minx) minx = tpcg.MinX();
        if (tpcg.MaxX() > maxx) maxx = tpcg.MaxX();
        if (tpcg.MinY() < miny) miny = tpcg.MinY();
        if (tpcg.MaxY() > maxy) maxy = tpcg.MaxY();
        if (tpcg.MinZ() < minz) minz = tpcg.MinZ();
        if (tpcg.MaxZ() > maxz) maxz = tpcg.MaxZ();
    }

    //x
    double dista = fabs(minx - position.X());
    double distb = fabs(position.X() - maxx);
    if ((position.X() > minx) && (position.X() < maxx) &&
        (dista > distance_buffer) && (distb > distance_buffer)) inside = true;
    //y
    dista = fabs(maxy - position.Y());
    distb = fabs(position.Y() - miny);
    if (inside && (position.Y() > miny) && (position.Y() < maxy) &&
        (dista > distance_buffer) && (distb > distance_buffer)) inside = true;
    else inside = false;
    //z
    dista = fabs(maxz - position.Z());
    distb = fabs(position.Z() - minz);
    if (inside && (position.Z() > minz) && (position.Z() < maxz) &&
        (dista > distance_buffer) && (distb > distance_buffer)) inside = true;
    else inside = false;
  }

  return inside;



}
