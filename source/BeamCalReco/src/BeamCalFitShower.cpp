/**
* @file BeamCalFitShower.cpp
* @brief Implementation of shower finding methods
* @author Andrey sapronov <sapronov@cern.ch>
* @version 0.0.1
* @date 2015-03-31
*/

#include "BeamCalFitShower.hh"
#include "BCRootUtilities.hh"
#include "BeamCalGeo.hh"
#include "BeamCalPadGeometry.hh"

#include <streamlog/loglevels.h>
#include <streamlog/streamlog.h>

// ROOT
#include "TMath.h"
#include "Minuit2/Minuit2Minimizer.h"
#include "Math/Functor.h"

#include <cmath>
#include <iostream>
#include <numeric>
#include <utility>

using std::vector;
using std::pair;

//=================================================================//
//                    BeamCalFitShower methods                     //
//=================================================================//

BeamCalFitShower::BeamCalFitShower(vector<EdepProfile_t*>& vep, const BCPadEnergies::BeamCalSide_t bc_side)
    : m_vep(vep),
      m_spotPads(vector<EdepProfile_t*>()),
      m_covInv(vector<double>()),
      m_spotEint(vector<double>()),
      m_BCG(nullptr),
      m_BCbackground(nullptr),
      m_BCside(bc_side),
      m_rhom(9.3),
      m_enTowerLimit(0.),
      m_towerChi2Limit(1.),
      m_startLayer(1),
      m_countingLayer(1),
      m_flagUncorr(false) {
  // hardcode now, make better later
  // m_rhom = 9.3; // Moliere radius (rho_M)
}


BeamCalFitShower::BeamCalFitShower(const BeamCalFitShower& fs)
		 : m_vep(fs.m_vep),
		   m_spotPads(fs.m_spotPads),
		   m_covInv(fs.m_covInv),
		   m_spotEint(fs.m_spotEint),
		   m_BCG(fs.m_BCG),
		   m_BCbackground(fs.m_BCbackground),
		   m_BCside(fs.m_BCside),
		   m_rhom(fs.m_rhom),
		   m_enTowerLimit(fs.m_enTowerLimit),
		   m_towerChi2Limit(fs.m_towerChi2Limit),
		   m_startLayer(fs.m_startLayer),
		   m_countingLayer(fs.m_countingLayer),
		   m_flagUncorr(fs.m_flagUncorr)
{}

BeamCalFitShower&
BeamCalFitShower::operator=(const BeamCalFitShower&fs)
{
  m_vep = fs.m_vep;
  m_BCG = fs.m_BCG;
  m_BCbackground = fs.m_BCbackground;

  m_enTowerLimit = fs.m_enTowerLimit;

  return *this;
}

double BeamCalFitShower::fitShower(double& theta, double& phi, double& en_shwr, double& chi2,
                                   std::map<int, double>& padIDsInCluster) {
  BCUtil::IgnoreRootError ire{};

  // select spot pads around most likely shower
  m_spotPads.clear();
  vector<int> pad_list;
  if ( this->selectSpotPads(pad_list) < 0 ){
    this->deleteSpotPads();

    theta = 0., phi = 0., en_shwr = 0., chi2 = 0.;
    return -1.;
  }

  // calculate inverse covariance matrix for spot pads
  /*
  if ( m_BCbackground->getPadsCovariance(pad_list, m_covInv, m_BCside) < 0 ){
    std::cout << "Falling back to uncorrelated errors in chi2 definition.\n";
    m_flagUncorr = true;
  }
  */
  // not using covariance for now
  m_flagUncorr = true;

  // fit the shower
  ROOT::Minuit2::Minuit2Minimizer minuit ( ROOT::Minuit2::kMigrad );
 
  minuit.SetMaxFunctionCalls(1000);
  minuit.SetMaxIterations(100);
  minuit.SetTolerance(0.1);
  //minuit.SetPrintLevel(3);
 
  const int npar = 4;
  ROOT::Math::Functor f(*this,npar); 
  //ROOT::Math::Functor f(&BeamCalFitShower::showerChi2,npar); 
 
  minuit.SetFunction(f);

  double R0 = m_spotPads.at(0)->padGeom->m_R;
  double dR0 = m_spotPads.at(0)->padGeom->m_dR;
  double phi0 = m_spotPads.at(0)->padGeom->m_phi;
  double dphi0 = m_spotPads.at(0)->padGeom->m_dphi;

  // estimate initial fit parameters
  double R_shr_center(0.), phi_shr_center(0.);
  double A0(0.), sig0(0.);
  this->estimateShowerPars(R_shr_center, phi_shr_center, A0, sig0);
  
  // set variables and their limits
  // make radius and phi limits a bit smaller than the pad size
  minuit.SetLimitedVariable(0,"R", R_shr_center, dR0/20., R0 - 0.499*dR0, R0 + 0.499*dR0);
  minuit.SetLimitedVariable(1,"phi", phi_shr_center, dphi0/20.,  phi0 - 0.5*dphi0, phi0 + 0.5*dphi0);
  minuit.SetLimitedVariable(2,"A", A0 , A0/20., 0.1*A0, 10.*A0);
  minuit.SetLimitedVariable(3,"sig", sig0, sig0/20., 0.1*sig0, 5*sig0);
 
  minuit.Minimize(); 

  // get the minimisation result
  const double *result=minuit.X();
  chi2 = minuit.MinValue();
  /*
  std::cout << minuit.Status() << std::endl;
  if ( minuit.Status()> 1 ) {
    this->deleteSpotPads();
    m_flagUncorr = false;

    return -1;
  }
  if ( chi2/m_spotPads.size() < 0.1 ) {
    this->deleteSpotPads();
    m_flagUncorr = false;

    return -1;
  }
		std::cout << std::setw(10) << std::setprecision(4);
  		std::cout << R_shr_center/m_BCG->getLayerZDistanceToIP(m_startLayer)*1000. << "\t" 
		          << result[0]/m_BCG->getLayerZDistanceToIP(m_startLayer)*1000.<< "\t" 
			  << (R0 - 0.499*dR0)/m_BCG->getLayerZDistanceToIP(m_startLayer)*1000.<< "\t" 
			  << (R0 + 0.499*dR0)/m_BCG->getLayerZDistanceToIP(m_startLayer)*1000.<< std::endl;
		std::cout << std::setw(10) << std::setprecision(4);
  		std::cout << phi_shr_center/M_PI*180.<< "\t" << result[1]/M_PI*180. << "\t" <<  (phi0 - 0.5*dphi0)/M_PI*180. << "\t" << (phi0 + 0.5*dphi0)/M_PI*180. << std::endl;
		std::cout << std::setw(10) << std::setprecision(4);
  		std::cout << A0 << "\t" << result[2] << "\t" << 0.1*A0 << "\t" << 10.*A0 << std::endl;
		std::cout << std::setw(10) << std::setprecision(4);
  		std::cout << sig0<< "\t" << result[3]<< "\t" << 0.1*sig0<< "\t" << 5*sig0 << std::endl;
 
  		//std::cout << chi2 << "\t" << result[0]<< "\t" <<result[1]<< "\t" <<result[2]<< "\t" <<result[3] << std::endl;
  */

  // call our chi2 function to calculate energies corresponding to minimum
  (*this)(result);  // wat?

  theta = result[0]/m_BCG->getLayerZDistanceToIP(m_startLayer)*1000.;
  phi = result[1]/M_PI*180.;
  //theta = 25.6359;
  //phic = 143.219*TMath::DegToRad();
  //phi = 143.219;
  en_shwr = accumulate(m_spotEint.begin(), m_spotEint.end(), 0.0);

  // get the padIDs from the projectedPadIds
  for (int padID : pad_list) {
    for (auto const& pair : m_vep[padID]->padIDs) {
      padIDsInCluster[pair.first] = pair.second;
    }
  }

		/*
  		vector<EdepProfile_t*>::iterator it_sp;
  		std::cout << "SPOTPADS edp:\t" ;
  		for ( it_sp = m_spotPads.begin() ;it_sp!=m_spotPads.end(); it_sp++){
  		  std::cout << (*it_sp)->totalEdep - (*it_sp)->bkgEdep<< "\t"  ;
  		}
  		std::cout  << std::endl;
  		
  		std::cout << "SPOTPADS fit:" ;
  		vector<double>::iterator it_ei = m_spotEint.begin();
  		for (;it_ei != m_spotEint.end(); it_ei++){
  		  std::cout<< "\t"  << *it_ei;
  		}
  		std::cout << std::endl;
  		std::cout << "chi2 fit:\t"<< chi2 << "\t" ;
		*/

  this->deleteSpotPads();
  m_flagUncorr = false;

  // calculate probability that this is our shower
  double prob = TMath::Prob(chi2, m_spotPads.size());
  return prob;
}

int BeamCalFitShower::selectSpotPads(vector<int> &pad_ids)
{
  streamlog_out(DEBUG5) << "Looking for spot pads" << std::endl;

  const double DEGRAD = M_PI/180.;

  // find tower with largest tower chi2, provided it is > 5000.
  vector<EdepProfile_t*>::iterator it_center = m_vep.begin();
  //double max_en = (*it_center)->en;
  double max_chi2 = (*it_center)->towerChi2;

  vector<EdepProfile_t*>::iterator it_ep = m_vep.begin();
  for (;it_ep!=m_vep.end(); it_ep++){
    double tower_chi2 = (*it_ep)->towerChi2;
    double en_tower = (*it_ep)->totalEdep - (*it_ep)->bkgEdep;
    if ( tower_chi2 > m_towerChi2Limit && en_tower > 0.7*m_enTowerLimit && max_chi2 < tower_chi2 ){
      it_center = it_ep;
      max_chi2 = tower_chi2;
      //max_en = en_tower;
    }
  }
    //std::cout << max_en<< "\t" << std::endl;

  // no pads with high enough chi2 found:
  if (it_center == m_vep.begin()) {
    streamlog_out(DEBUG5) << "No spot pads found" << std::endl;
    return -1;
  }
  // create entry for central pad
  double pext[6]; // tsk-tsk-tsk
  m_BCG->getPadExtentsById((*it_center)->id, pext);
  double dphi = pext[3]-pext[2];
  if (dphi < 0 ) dphi+= 360.; // in case pad extents over the -X axis
  (*it_center)->padGeom = new BeamCalPadGeometry(pext[4], pext[5]*DEGRAD, 
    pext[1]-pext[0], dphi*DEGRAD);
  (*it_center)->padGeom->m_isCentral = true;
  m_spotPads.push_back(*it_center);
  pad_ids.push_back((*it_center)->id);

  // collect pads within some Moliere radii of the maximum
  for ( it_ep = m_vep.begin() ;it_ep!=m_vep.end(); it_ep++){
    double pad_dist = m_BCG->getPadsDistance((*it_center)->id, (*it_ep)->id);
    double en_tower = (*it_ep)->totalEdep - (*it_ep)->bkgEdep;
    if ( pad_dist < 1.8*m_rhom && pad_dist > 0.01 && 
        en_tower > 0.1*m_enTowerLimit && en_tower > (*it_ep)->bkgSigma) {
      // if the pad is in the spot, assign its geometry too
      m_BCG->getPadExtentsById((*it_ep)->id, pext);
      dphi = pext[3]-pext[2];
      if (dphi < 0 ) dphi+= 360.; // in case pad extents over the -X axis
      (*it_ep)->padGeom = new BeamCalPadGeometry(pext[4], pext[5]*DEGRAD, pext[1]-pext[0], dphi*DEGRAD);
      (*it_ep)->padGeom->m_isCentral = false;
      m_spotPads.push_back(*it_ep);
      pad_ids.push_back((*it_ep)->id);
    }
  }

		/*
  		vector<EdepProfile_t*>::iterator it_sp;
  		std::cout << "SPOTPADS sig:\t0.\t" ;
  		for ( it_sp = m_spotPads.begin() ;it_sp!=m_spotPads.end(); it_sp++){
  		  std::cout << (*it_sp)->bkgSigma<< "\t"  ;
  		}
  		std::cout  << std::endl;
		*/

  streamlog_out(DEBUG5) << "Found spot pads with size " << m_spotPads.size() << std::endl;
  return m_spotPads.size();
}

//double BeamCalFitShower::showerChi2(double *par)
// this is strangest thing I've ever coded
double BeamCalFitShower::operator()(const double *par)
{
  const int np = m_spotPads.size();
  m_spotEint.assign(np,0.);

  vector<EdepProfile_t*>::iterator it_ep = m_spotPads.begin();
  for (;it_ep != m_spotPads.end(); it_ep++){
    (*it_ep)->padGeom->setLocalCoords(par[0], par[1]);
  }

  double dr(0.05*m_rhom);
  double ra(0), rb(dr);
  double rmax(3*m_rhom);
  double ga = par[2];
  //double ga = par[2]*TMath::Gaus(0.,0.,par[3]);
  double gm(0.), gb(0.);
  // simpson integration steps
  while (rb<rmax){
    double rm = (ra+rb)/2.;
    gm=par[2]*exp(-rm/par[3]);
    gb=par[2]*exp(-rb/par[3]);
    /*
    gm=par[2]*TMath::Gaus(rm,0.,par[3]);
    gb=par[2]*TMath::Gaus(rb,0.,par[3]);
    */
    // loop over spot pads
    for (it_ep = m_spotPads.begin();it_ep != m_spotPads.end(); it_ep++){
      // approximate arc at the central point
      double arc = (*it_ep)->padGeom->getArcWithin(rm);
      		//std::cout <<"arc="<< "\t" << arc<< "\n";
      m_spotEint.at(it_ep - m_spotPads.begin()) += 
        arc*dr/6.*(ga+4*gm+gb);
      //m_spotEint.at(it_ep - m_spotPads.begin()) += 
      //  arc*dr;
    }
    		// std::cout  << std::endl;
    ga = gb;
    ra = rb;
    rb += dr;
  }
  
  // calculate chi2 for integral and actual deposition
  // this piece implements convolution with covariance matrix:
  // chi2 = (Edep-Eint)^T x V x (Edep-Eint)
  // where Edep is a linear algebra vector of energy depositions in spot pads,
  // Eint is the same of energy integral,
  // V is the covariance matrix for the spot pads
  double chi2 = 0.;
  if ( !m_flagUncorr ) {
    for ( int i = 0 ; i < np; i++){
      double ExV(0.);
      for ( int j = 0 ; j < np; j++){
        double d_Ej = m_spotPads[j]->totalEdep - m_spotPads[j]->bkgEdep - m_spotEint[j];
        ExV += d_Ej*m_covInv[j*np+i];
        //std::cout <<m_spotPads[i]->id<< "\t" <<m_spotPads[j]->id<< "\t" << m_covInv[j*np+i] << std::endl;
      }
      //std::cout <<  std::endl;
      double d_Ei = m_spotPads[i]->totalEdep - m_spotPads[i]->bkgEdep - m_spotEint[i];
      chi2 += ExV*d_Ei;
    }
  } else {
    for (it_ep = m_spotPads.begin();it_ep != m_spotPads.end(); it_ep++){
      chi2 += pow(((*it_ep)->totalEdep - (*it_ep)->bkgEdep - m_spotEint.at(it_ep-m_spotPads.begin()))/(*it_ep)->bkgSigma,2);
    }
  }

		/*
  		std::cout << "SPOTPADS:\t"<< chi2 << "\t" ;
  		vector<double>::iterator it_ei = m_spotEint.begin();
  		for (;it_ei != m_spotEint.end(); it_ei++){
  		  std::cout<< "\t"  << *it_ei;
  		}
  		std::cout << std::endl;
  		//exit(1);
		*/
		

  return chi2;
}

  
void BeamCalFitShower::deleteSpotPads()
{
  // delete pad geometries from spot pads
  vector<EdepProfile_t*>::iterator it_sp;
  for ( it_sp = m_spotPads.begin() ;it_sp!=m_spotPads.end(); it_sp++){
    delete (*it_sp)->padGeom;
  }
 
  // exclude towers of the found shower from next search
  vector<EdepProfile_t*>::iterator it_ep = m_vep.end()-1;
  for (;it_ep >= m_vep.begin(); it_ep--){
    for (it_sp = m_spotPads.begin();it_sp != m_spotPads.end(); it_sp++){
      if ( (*it_ep)->id == (*it_sp)->id )
	it_ep = m_vep.erase(it_ep);
    }
  }
}


void BeamCalFitShower::estimateShowerPars(double &rc, double &phic, double &A0, double &sig0)
{
  rc = 0.;
  phic = 0.;
  double logesum(0.);
  vector<EdepProfile_t*>::iterator it_ep = m_spotPads.begin();
  for (it_ep = m_spotPads.begin();it_ep != m_spotPads.end(); it_ep++){
    double esh = log((*it_ep)->totalEdep - (*it_ep)->bkgEdep);
    rc +=  esh * (*it_ep)->padGeom->m_R;
    phic += esh * (*it_ep)->padGeom->m_phi;
    logesum += esh;
  }

  if (0. == logesum ){
    std::cout << "Warning in BeamCalFitShower: unable to estimate shower center, \n" 
              << "will use hottest pad center coordinates as a fit starting point." << std::endl;
    rc = m_spotPads.at(0)->padGeom->m_R;
    phic = m_spotPads.at(0)->padGeom->m_phi;
  } else {
    rc /= logesum;
    phic /= logesum;
  }

  //rc = 32.39*m_BCG->getLayerZDistanceToIP(m_startLayer)/1000.;
  //phic = 289.75*TMath::DegToRad();
  //phic = 2.4996;
  //std::cout << 25.6359 *m_BCG->getLayerZDistanceToIP(m_startLayer)/1000. << std::endl;

  double esum(0.);
  for (it_ep = m_spotPads.begin();it_ep != m_spotPads.end(); it_ep++){
    esum+=(*it_ep)->totalEdep - (*it_ep)->bkgEdep;
  }

  EdepProfile_t* epc = m_spotPads.at(0); // central pad profile
  double delt = 0.5*epc->padGeom->m_dR;
  sig0 = 0.2*m_rhom;
  A0 = 0.2*2.*M_PI*(epc->totalEdep - epc->bkgEdep)/m_rhom/(1-exp(-delt/m_rhom)) ;
}
