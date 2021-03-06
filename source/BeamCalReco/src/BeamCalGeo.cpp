#include "BeamCalGeo.hh"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <stdexcept>

//Return the double[6] pointer with innerRadius, Outerradius, Phi1 and Phi2, middle radius, middle phi
void BeamCalGeo::getPadExtents(int cylinder, int sector, double *extents) const {

  double radius1, radius2;
  radius1 = this->getRadSegmentation()[cylinder];
  radius2 = this->getRadSegmentation()[cylinder+1];

  extents[0] = radius1;
  extents[1] = radius2;

  extents[2] = getPadPhi(cylinder, sector) - 360.0 / double(getPadsInRing(cylinder));
  extents[3] = getPadPhi(cylinder, sector+1) - 360.0 / double(getPadsInRing(cylinder));
  extents[4] = (extents[0] + extents[1])/2;

  //this bit takes care of pads at the edge of 360 to 0
  if( fabs( extents[2] - extents[3] ) < 20 ) {
    extents[5] = (extents[2] + extents[3])/2.0;
  } else {
    extents[5] = ( (extents[2] + extents[3] - 360) / 2.0 + 360 );
    if( extents[5] > 360.0 ) {
      extents[5] -= 360.0;
    }

  }  

  if( extents[2] >= 360.0 ) {
    extents[2] -= 360.0;
  }

  if( extents[3] >= 360.0 ) {
    extents[3] -= 360.0;
  }

  if( extents[5] >= 360.0 ) {
    extents[5] -= 360.0;
  }

  if( extents[2] <= 0.0 ) {
    extents[2] += 360.0;
  }

  if( extents[3] <= 0.0 ) {
    extents[3] += 360.0;
  }

  if( extents[5] <= 0.0 ) {
    extents[5] += 360.0;
  }

}//GetPadExtents


double BeamCalGeo::getPadMiddlePhi(int cylinder, int sector) const {
  double extents[6];
  getPadExtents(cylinder, sector, extents);
  // if( cylinder == 6 && sector == 15 ) {
  //   std::cout << __func__  << "  " << extents[5] - 180 << std::endl;
  // }
  return extents[5];
}

double BeamCalGeo::getPadMiddleR(int cylinder, int sector) const {
  double extents[6];
  getPadExtents(cylinder, sector, extents);
  return extents[4];
}

double BeamCalGeo::getPadMiddleTheta(int layer, int cylinder, int sector) const {
  double extents[6];
  getPadExtents(cylinder, sector, extents);
  return atan(extents[4]/getLayerZDistanceToIP(layer));
}

/**
 * Calculate the theta from the ring ID, ring starts at 0
 */

double BeamCalGeo::getThetaFromRing(int layer, double averageRing) const  {
  const double radiusStep = getBCOuterRadius() - getBCInnerRadius();
  const double radius = getBCInnerRadius() + ( averageRing + 0.5 )  * ( ( radiusStep  ) / double(getBCRings()) );
  return atan( radius / getLayerZDistanceToIP(layer) );
}

double BeamCalGeo::getThetaFromRing(int layer, int ring) const  {
  const double radius = (getRadSegmentation()[ring+1]+getRadSegmentation()[ring])*0.5;
  return atan( radius / getLayerZDistanceToIP(layer) );
}

double BeamCalGeo::getThetaFromRing(int ring) const  {
  const double radius = (getRadSegmentation()[ring+1]+getRadSegmentation()[ring])*0.5;
  return atan( radius / getBCZDistanceToIP() );
}

//ARG
double BeamCalGeo::getPadPhi(int ring, int pad) const {
  const double RadToDeg = 180.0 / M_PI;
  double phi = 0;
  if( ring < getFirstFullRing()) {
    phi += getPhiOffset();
    double deltaPhi = (360.0 - RadToDeg * getFullKeyHoleCutoutAngle())/double(getPadsInRing(ring));
    phi += deltaPhi * (0.5 + double(pad));
  } else {
    double deltaPhi = 360.0 / double(getPadsInRing(ring));
    //Pads still start at the top of the cutout! which is negative -180 + half cutoutangle
    phi += getPhiOffset() + deltaPhi * (0.5 + double(pad));
  }

  phi  += 180.0;
  if( phi > 360.0 ) {
    phi -= 360.0;
  }

  //  std::cout << std::setw(10) << ring << std::setw(10) << pad << std::setw(10) << phi  << std::endl;
  return phi;
}

double BeamCalGeo::getPadPhi(int globalPandIndex) const {
  int layer, ring, pad;
  getLayerRingPad(globalPandIndex, layer, ring, pad);
  return getPadPhi(ring, pad);
}




/**
 * Returns true if the pads are in adjacent cells in the same ring
 * or if they are in adjacent rings, then it will return true if the overlap in phi is smaller than
 * some amount of degrees...
 * do the layers have to be the same? --> use boolean flag
 * What about adjacent layers and the same index? we are going to write a second function for those, if we have to
 * Pads are Neighbours if they are in the same Ring, but have Pads with a difference of one
 * What about the keyhole cutout? They will not be considered to be neighbours
 */

bool BeamCalGeo::arePadsNeighbours(int globalPadIndex1, int globalPadIndex2, bool mustBeInSameLayer) const {
  int layerIndex1, layerIndex2, ringIndex1, ringIndex2, padIndex1, padIndex2;
  getLayerRingPad(globalPadIndex1, layerIndex1, ringIndex1, padIndex1);
  getLayerRingPad(globalPadIndex2, layerIndex2, ringIndex2, padIndex2);

  if( mustBeInSameLayer and ( not ( layerIndex1 == layerIndex2 ) ) ) {
    //    std::cout << "Not Neighbours: Layers differ"  << std::endl;
    return false;
  }
  //If they are in the same ring
  if( ringIndex1 == ringIndex2 ) {

    if( 
       //The index of neighbouring pads differs by 1
       ( std::abs( padIndex1 - padIndex2 ) <= 1 ) or
       // or one is zero and the other is m_segments[ringIndex1] - 1
       (  ringIndex1 >= getFirstFullRing() && ( ( std::abs( padIndex1 - padIndex2 ) + 1 )  == getPadsInRing(ringIndex1) )  )
	) {
      // std::cout << "Neighbours: Adjacent pads in same ring"  
      // 		<< std::setw(5) << padIndex1
      // 		<< std::setw(5) << padIndex2
      // 		<< std::endl;
      return true;
    } else { //if they are in the same ring, but not neighbours, we can bail out here
      // std::cout << "Not Neighbours: Same ring but not adjacent pads" 
      // 		<< std::setw(5) << padIndex1
      // 		<< std::setw(5) << padIndex2
      // 		<< std::endl;
      return false;
    }
  }//if in the same ring

  //adjacent rings' indices differ by 1
  if( std::abs( ringIndex1 - ringIndex2 ) == 1 ) {

    //if they have the same number of Rings in the pads it is easy
    const int nSegments1(this->getNSegments(  )[ringIndex1]);
    const int nSegments2(this->getNSegments(  )[ringIndex2]);

    if ( nSegments1  == nSegments2 ) {
      return (padIndex1 == padIndex2);
    }

    const int whichSegment1 = padIndex1 / nSegments1;
    const int whichSegment2 = padIndex2 / nSegments2;
    if ( whichSegment1 != whichSegment2 ) { return false; }	          
								          
    //if they are in the same segment we compare the modulos	          
    const int padIndexMod1 =  padIndex1 % nSegments1;
    const int padIndexMod2 =  padIndex2 % nSegments2;
    //std::cout << "Different rings, different segments"  << std::endl;
    if ( ringIndex1 < ringIndex2 ) {
      return ( ((padIndexMod2 - padIndexMod1) == 0 ) || ((padIndexMod2 - padIndexMod1) == 1 ) );
    } else {
      return ( ((padIndexMod1 - padIndexMod2) == 0 ) || ((padIndexMod1 - padIndexMod2) == 1 ) );
    }


    // //Angles are in Degrees!
    // double phi1 = getPadPhi(ringIndex1, padIndex1);
    // double phi2 = getPadPhi(ringIndex2, padIndex2);

    // if( fabs( phi1 - phi2 ) < 18.0 ) {
    //   // std::cout << "Neighbours: Adjacent pads in different ring"  
    //   // 		<< std::setw(10) << phi1
    //   // 		<< std::setw(10) << phi2
    //   // 		<< std::endl;
    //   return true;
    // } else {
    //   // std::cout << "Not Neighbours: Non adjacent pads in different ring"  
    //   // 		<< std::setw(10) << phi1
    //   // 		<< std::setw(10) << phi2
    //   // 		<< std::endl;
    //   return false;
    // }  

  }//in adjacent rings
  return false;

}


int BeamCalGeo::getPadsInRing( int ring ) const {
    if ( ring < getFirstFullRing() ) {
      return getNSegments()[ring] * getSymmetryFold();
    } else {
      int segmentsInRing = getSymmetryFold()*getNSegments()[ring];
      double deltaPhi = ( 2.0*M_PI - getDeadAngle())/double(segmentsInRing);
      int additionalSegments = (int)((getDeadAngle()/deltaPhi));
      //if the new segments would not cover the full dead angle range, add one
      //add 1e-5 for floating point precision
      if ( additionalSegments*deltaPhi+1e-5 < getDeadAngle() ) {
	additionalSegments++;
      }
      return segmentsInRing + additionalSegments;
    }
}


int BeamCalGeo::getPadsBeforeRing( int ring ) const {
  int nRings(1);
  //  std::cout << "GetPadsBefore Ring " << ring  << std::endl;
  for (int i = 0; i < ring-1; ++i) {//we want the number of pads _before_ the ring
    nRings += getPadsInRing(i);
  }
  //std::cout << "getPadsBefore " << nRings  << std::endl;
  return nRings;
}

int BeamCalGeo::getPadsPerBeamCal() const{
  return getPadsBeforeRing( getBCRings()+1 ) * getBCLayers();
}

int BeamCalGeo::getPadsPerLayer() const {
  return getPadsBeforeRing( getBCRings()+1 );
}

//layers start at 1, ring and pad start at 0
int BeamCalGeo::getPadIndex(int layer, int ring, int pad) const {
  if( layer < 1 || getBCLayers() < layer) {//starting at 1 ending at nLayers
    throw std::out_of_range("getPadIndex: Layer out of range:");
  } else if(ring < 0 || getBCRings() <= ring) {//starting at 0, last entry is nRings-1
    throw std::out_of_range("getPadIndex: Ring out of range:");
  } else if( pad < 0 || getPadsInRing(ring) <= pad ) {//starting at 0
    throw std::out_of_range("getPadIndex: Pad out of range:");
  }

  return (layer-1) * (getPadsPerLayer()) + getPadsBeforeRing(ring) + (pad);
}


void BeamCalGeo::getLayerRingPad(int padIndex, int& layer, int& ring, int& pad) const{

  //how often does nPadsPerLayer fit into padIndex;
  //layer starts at 1!
  layer = getLayer(padIndex);// ( padIndex / getPadsPerLayer() ) + 1;
  ring = getRing(padIndex);

  const int ringIndex(padIndex % getPadsPerLayer());
  pad = ringIndex - getPadsBeforeRing(ring);

#ifdef DEBUG
  if (padIndex != this->getPadIndex(layer, ring, pad) ) {
    std::stringstream error;
    error << "PadIndex " 
  	  << std::setw(7) << padIndex
  	  << std::setw(7) << this->getPadIndex(layer, ring, pad);
    throw std::logic_error(error.str());
  }
#endif

  return;

}//getLayerRingPad

int BeamCalGeo::getRing(int padIndex) const {
  int ringIndex = padIndex % getPadsPerLayer();
  int ring = 0;
  while ( getPadsBeforeRing( ring ) <= ringIndex) { ++ring; }
  // std::vector<int>::const_iterator element =  std::upper_bound(m_PadsBeforeRing.begin()+1, m_PadsBeforeRing.end(),
  // 							       padIndex % getPadsPerLayer());
  return ring - 1 ;
}

int BeamCalGeo::getLocalPad(int padIndex) const {
  int layer, ring, pad;
  getLayerRingPad(padIndex, layer, ring, pad);
  return pad;
}


int BeamCalGeo::getLayer(int padIndex) const {
  //layer starts at 1
  // AS: GEAR documentation says that it starts at 0 and there is boundary
  // violation otherwise.
  // Because there are a lot of things done in assumption that it starts form 1, 
  // I just return last layer when it's out of boundaries
  int lrnum = ( padIndex / getPadsPerLayer() ) + 1;
  if (lrnum > this->getBCLayers()-1) lrnum--;

  return  lrnum;
}

 int BeamCalGeo::getFirstFullRing()   const {
  int ring = 0;
  while ( getCutout() > getRadSegmentation()[ring] ) { ++ring; }
  return ring;
}

void BeamCalGeo::getPadExtentsById(int globalPadIndex, double *extents) const {
  int layer=0, ring=0, pad=0;
  getLayerRingPad( globalPadIndex, layer, ring, pad);
  this->getPadExtents(ring, pad, extents);
}


double BeamCalGeo::getPadsDistance(int padIndex1, int padIndex2) const {
  const double DEGRAD=M_PI/180.;
  static double e1[6], e2[6];
  static int prev_pi1(-1), prev_pi2(-1);
  if ( padIndex1 != prev_pi1) getPadExtentsById(padIndex1, e1), prev_pi1 = padIndex1;
  if ( padIndex2 != prev_pi2) getPadExtentsById(padIndex2, e2), prev_pi2 = padIndex2;
  //std::cout << padIndex1 << "\t" << padIndex2 << "\t" << e2[4] << "\t" << e2[5] << std::endl;

  double dx = e1[4]*cos(e1[5]*DEGRAD) - e2[4]*cos(e2[5]*DEGRAD);
  double dy = e1[4]*sin(e1[5]*DEGRAD) - e2[4]*sin(e2[5]*DEGRAD);
  double d = sqrt(dx*dx+dy*dy);
  return d;
}

std::ostream& operator<<(std::ostream& o, const BeamCalGeo& bcg) {
  o << "Radii" << std::setw(13) << bcg.getBCInnerRadius() << std::setw(13) << bcg.getBCOuterRadius()
    << "\nRadial Segmentation:\n";
  for (double r : bcg.getRadSegmentation()) {
    o << std::setw(10) << r;
  }
  o << "\nZDistances";
  for (int i = 0; i < bcg.getBCLayers(); ++i) {
    o << std::setw(13) << bcg.getLayerZDistanceToIP(i);
  }

  o << "\nNRings" << std::setw(13)  << bcg.getBCRings() << "   nRadValues " << bcg.getRadSegmentation().size()
    << "\nFirstFullRing" << std::setw(15)  << bcg.getFirstFullRing()
    << " CutOutRadius [mm]" << std::setw(15)  << bcg.getCutout()
    << " KeyHoleCutoutAngle [rad]" << std::setw(15)  << bcg.getFullKeyHoleCutoutAngle()
    << "\nPhi Offset [Deg] " << std::setw(15) << bcg.getPhiOffset()
    << "\nPhi Segments";
  for (int i : bcg.getNSegments()) {
    o << std::setw(5) << i;
  }
  o << "\nTheta Segments:\n";
  for (int l = 0; l < bcg.getBCLayers(); ++l) {
    o << "Layer: " << l << ": ";
    for (int i = 0; i < bcg.getBCRings(); ++i) {
      o << std::setw(10) << bcg.getThetaFromRing(l, i);
    }
    o << std::endl;
  }

  o << std::endl;
  return o;
}
