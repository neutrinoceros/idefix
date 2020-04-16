#include "idefix.hpp"
#include "setup.hpp"

/*********************************************/
/**
Customized random number generator
Allow one to have consistant random numbers
generators on different architectures.
**/
/*********************************************/


// Default constructor
Setup::Setup() {}

// Initialisation routine. Can be used to allocate
// Arrays or variables which are used later on
Setup::Setup(Input &input, Grid &grid, DataBlock &data) {

}

// This routine initialize the flow
// Note that data is on the device.
// One can therefore define locally 
// a datahost and sync it, if needed
void Setup::InitFlow(DataBlock &data) {
    // Create a host copy
    DataBlockHost d(data);


    for(int k = 0; k < d.np_tot[KDIR] ; k++) {
        for(int j = 0; j < d.np_tot[JDIR] ; j++) {
            for(int i = 0; i < d.np_tot[IDIR] ; i++) {
                
                d.Vc(RHO,k,j,i) = (d.x[IDIR](i)<50.0) ? 1.0 : 0.125;
                d.Vc(VX1,k,j,i) = ZERO_F;
#if HAVE_ENERGY 
                d.Vc(PRS,k,j,i) = (d.x[IDIR](i)<50.0) ? 1.0 : 0.1;
#endif
                d.Vs(BX1s,k,j,i) = 0.75;
		d.Vc(BX1,k,j,i) = 0.75;
                d.Vc(BX2,k,j,i) = (d.x[IDIR](i)<50.0) ? 1.0 : -1.0;
		#if DIMENSIONS >= 2
		d.Vs(BX2s,k,j,i) = (d.xl[IDIR](i)<50.0) ? 1.0 : -1.0;
		#endif
            }
        }
    }
    
    // Send it all, if needed
    d.SyncToDevice();
}

// Analyse data to produce an output
void Setup::MakeAnalysis(DataBlock & data, real t) {

}

// User-defined boundaries
void Setup::SetUserdefBoundary(DataBlock& data, int dir, BoundarySide side, real t) {

}


// Do a specifically designed user step in the middle of the integration
void ComputeUserStep(DataBlock &data, real t, real dt) {

}
