#include "../idefix.hpp"
#include "physics.hpp"

/********************************
 * Local Kokkos Inline function *
 * ******************************/

KOKKOS_INLINE_FUNCTION void K_Flux(real F[], real V[], real U[], real C2Iso, const int dir) {
    int VXn = VX1+dir;
    int MXn = VXn;

    F[RHO] = U[VXn];

    EXPAND(F[MX1] = U[MX1]*V[VXn]; ,
           F[MX2] = U[MX2]*V[VXn]; ,
           F[MX3] = U[MX3]*V[VXn];)


#if HAVE_ENERGY
    F[ENG]     = (U[ENG] + V[PRS])*V[VXn];
    F[MXn]     += V[PRS];

#elif EOS == ISOTHERMAL
    // Add back pressure in the flux
    F[MXn]     += C2Iso * V[RHO];
#endif
} 

KOKKOS_INLINE_FUNCTION void K_ConsToPrim(real Vc[], real Uc[], real gamma_m1) {
    

    Vc[RHO] = Uc[RHO];

    EXPAND( Vc[VX1] = Uc[MX1]/Uc[RHO]; ,
            Vc[VX2] = Uc[MX2]/Uc[RHO]; ,
            Vc[VX3] = Uc[MX3]/Uc[RHO];)     

#if HAVE_ENERGY
    real kin;
    kin = 0.5 / Uc[RHO] * (EXPAND(    Uc[MX1]*Uc[MX1] , 
                                    + Uc[MX2]*Uc[MX2] ,
                                    + Uc[MX3]*Uc[MX3] ));

    Vc[PRS] = gamma_m1 * (Uc[ENG] - kin);
#endif  // Have_energy
}

KOKKOS_INLINE_FUNCTION void K_PrimToCons(real Uc[], real Vc[], real gamma_m1) {

    Uc[RHO] = Vc[RHO];

    EXPAND( Uc[MX1] = Vc[VX1]*Vc[RHO]; ,
            Uc[MX2] = Vc[VX2]*Vc[RHO]; ,
            Uc[MX3] = Vc[VX3]*Vc[RHO];)

#if HAVE_ENERGY

    Uc[ENG] = Vc[PRS] / gamma_m1 
                + 0.5 * Vc[RHO] * EXPAND(  Vc[VX1]*Vc[VX1] , 
                                         + Vc[VX2]*Vc[VX2] ,
                                         + Vc[VX3]*Vc[VX3] ); 
#endif  // Have_energy

}




Physics::Physics(Input &input, Setup &setup) {
    Kokkos::Profiling::pushRegion("Physics::Physics(DataBock)");

    this->gamma = 5.0/3.0;
    this->C2Iso = 1.0;

    this->mySetup=setup;

    Kokkos::Profiling::popRegion();
}

Physics::Physics() {

}


// Convect Conservative to Primitive variable
void Physics::ConvertConsToPrim(DataBlock & data) {

    Kokkos::Profiling::pushRegion("Physics::ConvertConsToPrim");

    IdefixArray4D<real> Vc = data.Vc;
    IdefixArray4D<real> Uc = data.Uc;
    real gamma_m1=this->gamma-ONE_F;

    idefix_for("ConsToPrim",0,data.np_tot[KDIR],0,data.np_tot[JDIR],0,data.np_tot[IDIR],
                        KOKKOS_LAMBDA (int k, int j, int i) 
            {
                real U[NVAR];
                real V[NVAR];
                for(int nv = 0 ; nv < NVAR; nv++) {
                    U[nv] = Uc(nv,k,j,i); 
                }

                K_ConsToPrim(V,U,gamma_m1);

                for(int nv = 0 ; nv<NVAR; nv++) {
                    Vc(nv,k,j,i) = V[nv];
                }
            });

    Kokkos::Profiling::popRegion();

}


// Convert Primitive to conservative variables
void Physics::ConvertPrimToCons(DataBlock & data) {

    Kokkos::Profiling::pushRegion("Physics::ConvertPrimToCons");

    IdefixArray4D<real> Vc = data.Vc;
    IdefixArray4D<real> Uc = data.Uc;
    real gamma_m1=this->gamma-ONE_F;

    idefix_for("ConvertPrimToCons",0,data.np_tot[KDIR],0,data.np_tot[JDIR],0,data.np_tot[IDIR],
                        KOKKOS_LAMBDA (int k, int j, int i) 
            {
                real U[NVAR];
                real V[NVAR];
                for(int nv = 0 ; nv < NVAR; nv++) {
                    V[nv] = Vc(nv,k,j,i); 
                }

                K_PrimToCons(U,V,gamma_m1);

                for(int nv = 0 ; nv<NVAR; nv++) {
                    Uc(nv,k,j,i) = U[nv];
                }
            });

    Kokkos::Profiling::popRegion();
}


// Build a left and right extrapolation of the primitive variables along direction dir

// These functions extrapolate the cell prim vars to the faces. Definitions are as followed
//
// |       cell i-1               interface i          cell i
// |-----------------------------------|------------------------------------||
//          Vc(i-1)           PrimL(i)  PrimR(i)       Vc(i)

void Physics::ExtrapolatePrimVar(DataBlock &data, int dir) {
    int ioffset,joffset,koffset;

    Kokkos::Profiling::pushRegion("Physics::ExtrapolatePrimVar");
    ioffset=joffset=koffset=0;
    // Determine the offset along which we do the extrapolation
    if(dir==IDIR) ioffset=1;
    if(dir==JDIR) joffset=1;
    if(dir==KDIR) koffset=1;

    IdefixArray4D<real> Vc = data.Vc;
    IdefixArray4D<real> PrimL = data.PrimL;
    IdefixArray4D<real> PrimR = data.PrimR;


#if ORDER == 1

    idefix_for("ExtrapolatePrimVar",0,NVAR,data.beg[KDIR],data.end[KDIR]+koffset,data.beg[JDIR],data.end[JDIR]+joffset,data.beg[IDIR],data.end[IDIR]+ioffset,
                        KOKKOS_LAMBDA (int n, int k, int j, int i) 
            {   
                
                PrimL(n,k,j,i) = Vc(n,k-koffset,j-joffset,i-ioffset);
                PrimR(n,k,j,i) = Vc(n,k,j,i);
            });

#elif ORDER == 2
    idefix_for("ExtrapolatePrimVar",0,NVAR,data.beg[KDIR]-koffset,data.end[KDIR]+koffset,data.beg[JDIR]-joffset,data.end[JDIR]+joffset,data.beg[IDIR]-ioffset,data.end[IDIR]+ioffset,
                        KOKKOS_LAMBDA (int n, int k, int j, int i) 
            {
                real dvm = Vc(n,k,j,i)-Vc(n,k-koffset,j-joffset,i-ioffset);
                real dvp = Vc(n,k+koffset,j+joffset,i+ioffset) - Vc(n,k,j,i);

                // Van Leer limiter
                real dv = (dvp*dvm > ZERO_F ? TWO_F*dvp*dvm/(dvp + dvm) : ZERO_F);

                PrimL(n,k+koffset,j+joffset,i+ioffset) = Vc(n,k,j,i) + HALF_F*dv;
                PrimR(n,k,j,i) = Vc(n,k,j,i) - HALF_F*dv;

            });
#else   
        #error ORDER should be 1 or 2
#endif



    Kokkos::Profiling::popRegion();
}

// Compute Riemann fluxes from states
void Physics::CalcRiemannFlux(DataBlock & data, int dir) {
    int ioffset,joffset,koffset;

    Kokkos::Profiling::pushRegion("Physics::CalcRiemannFlux");
    ioffset=joffset=koffset=0;
    // Determine the offset along which we do the extrapolation
    if(dir==IDIR) ioffset=1;
    if(dir==JDIR) joffset=1;
    if(dir==KDIR) koffset=1;

    IdefixArray4D<real> PrimL = data.PrimL;
    IdefixArray4D<real> PrimR = data.PrimR;
    IdefixArray4D<real> Flux = data.FluxRiemann;
    IdefixArray1D<real> dx = data.dx[dir];
    IdefixArray3D<real> invDt = data.InvDtHyp;

    real gamma_m1=this->gamma-ONE_F;
    real C2Iso = this->C2Iso;

    idefix_for("CalcRiemannFlux",data.beg[KDIR],data.end[KDIR]+koffset,data.beg[JDIR],data.end[JDIR]+joffset,data.beg[IDIR],data.end[IDIR]+ioffset,
                        KOKKOS_LAMBDA (int k, int j, int i) 
            {
                int VXn = VX1+dir;
                int MXn = VXn;
                // Primitive variables
                real vL[NVAR];
                real vR[NVAR];
                real vRL[NVAR];

                // Conservative variables
                real uL[NVAR];
                real uR[NVAR];

                // Flux (left and right)
                real fluxL[NVAR];
                real fluxR[NVAR];

                // Signal speeds
                real cRL, cmax;

                // 1-- Store the primitive variables on the left, right, and averaged states
                for(int nv = 0 ; nv < NVAR; nv++) {
                    vL[nv] = PrimL(nv,k,j,i);
                    vR[nv] = PrimR(nv,k,j,i);
                    vRL[nv]=HALF_F*(vL[nv]+vR[nv]);
                }

                // 2-- Compute the conservative variables
                K_PrimToCons(uL, vL, gamma_m1);
                K_PrimToCons(uR, vR, gamma_m1);

                // 3-- Compute the left and right fluxes
                K_Flux(fluxL, vL, uL, C2Iso, dir);
                K_Flux(fluxR, vR, uR, C2Iso, dir);

                // 4-- Get the wave speed
            #if HAVE_ENERGY
                cRL = SQRT((gamma_m1+ONE_F)*(vRL[PRS]/vRL[RHO]));
            #else
                cRL = SQRT(C2Iso);
            #endif
                cmax = FMAX(FABS(vRL[VXn]+cRL),FABS(vRL[VXn]-cRL));

                // 5-- Compute the flux from the left and right states
                for(int nv = 0 ; nv < NVAR; nv++) {
                    Flux(nv,k,j,i) = HALF_F*(fluxL[nv]+fluxR[nv] - cmax*(uR[nv]-uL[nv]));
                }

                //6-- Compute maximum dt for this sweep
                const int ig = ioffset*i + joffset*j + koffset*k;

                invDt(k,j,i) += cmax/dx(ig);

            });

    Kokkos::Profiling::popRegion();

}

// Compute the right handside in direction dir from conservative equation, with timestep dt
void Physics::CalcRightHandSide(DataBlock &data, int dir, real dt) {

    Kokkos::Profiling::pushRegion("Physics::CalcRightHandSide");
    IdefixArray4D<real> Uc = data.Uc;
    IdefixArray1D<real> dx = data.dx[dir];
    IdefixArray4D<real> Flux = data.FluxRiemann;

    int ioffset,joffset,koffset;
    ioffset=joffset=koffset=0;
    // Determine the offset along which we do the extrapolation
    if(dir==IDIR) ioffset=1;
    if(dir==JDIR) joffset=1;
    if(dir==KDIR) koffset=1;

    idefix_for("CalcRightHandSide",0,NVAR,data.beg[KDIR],data.end[KDIR],data.beg[JDIR],data.end[JDIR],data.beg[IDIR],data.end[IDIR],
        KOKKOS_LAMBDA (int n, int k, int j, int i) {
            
            const int ig = ioffset*i + joffset*j + koffset*k;

            Uc(n,k,j,i) = Uc(n,k,j,i) - dt / dx(ig) * (Flux(n, k+koffset, j+joffset, i+ioffset) - Flux(n, k, j, i));

        });

    Kokkos::Profiling::popRegion();
}


// Set Boundary conditions
void Physics::SetBoundary(DataBlock &data, real t) {

    Kokkos::Profiling::pushRegion("Physics::SetBoundary");

    IdefixArray4D<real> Vc = data.Vc;
    int ibeg,iend,jbeg,jend,kbeg,kend;
    int ioffset,joffset,koffset;
    int ighost,jghost,kghost;

    ighost = data.nghost[IDIR];
    jghost = data.nghost[JDIR];
    kghost = data.nghost[KDIR];

    // X1 boundary conditions
    

    for(int dir=0 ; dir < DIMENSIONS ; dir++ ) {

        ioffset = (dir == IDIR) ? data.np_int[IDIR] : 0;
        joffset = (dir == JDIR) ? data.np_int[JDIR] : 0;
        koffset = (dir == KDIR) ? data.np_int[KDIR] : 0;


        // left boundary
        ibeg=0;
        iend= (dir == IDIR) ? ighost : data.np_tot[IDIR];
        jbeg=0;
        jend= (dir == JDIR) ? jghost : data.np_tot[JDIR];
        kbeg=0;
        kend= (dir == KDIR) ? kghost : data.np_tot[KDIR];

        switch(data.lbound[dir]) {
            case periodic:
                idefix_for("BoundaryBegPeriodic",0,NVAR,kbeg,kend,jbeg,jend,ibeg,iend,
                    KOKKOS_LAMBDA (int n, int k, int j, int i) {
                
                        Vc(n,k,j,i) = Vc(n,k+koffset,j+joffset,i+ioffset);
                    });
                break;
            case outflow:
                idefix_for("BoundaryBegOutflow",0,NVAR,kbeg,kend,jbeg,jend,ibeg,iend,
                    KOKKOS_LAMBDA (int n, int k, int j, int i) {
                        int iref= (dir==IDIR) ? ighost : i;
                        int jref= (dir==JDIR) ? jghost : j;
                        int kref= (dir==KDIR) ? kghost : k;

                        Vc(n,k,j,i) = Vc(n,kref,jref,iref);
                    });
                break;
            default:
                std::stringstream msg ("Boundary condition type is not yet implemented");
                IDEFIX_ERROR(msg);
        }

        // right boundary
        ibeg= (dir == IDIR) ? ioffset + ighost : 0;
        iend = data.np_tot[IDIR];
        jbeg= (dir == JDIR) ? joffset + jghost : 0;
        jend = data.np_tot[JDIR];
        kbeg= (dir == KDIR) ? koffset + kghost : 0;
        kend = data.np_tot[KDIR];

        switch(data.rbound[dir]) {
            case periodic:
                idefix_for("BoundaryEndPeriodic",0,NVAR,kbeg,kend,jbeg,jend,ibeg,iend,
                    KOKKOS_LAMBDA (int n, int k, int j, int i) {
                
                        Vc(n,k,j,i) = Vc(n,k-koffset,j-joffset,i-ioffset);
                    });
                break;
            case outflow:
                idefix_for("BoundaryEndOutflow",0,NVAR,kbeg,kend,jbeg,jend,ibeg,iend,
                    KOKKOS_LAMBDA (int n, int k, int j, int i) {
                        int iref= (dir==IDIR) ? ighost + ioffset - 1 : i;
                        int jref= (dir==JDIR) ? jghost + joffset - 1 : j;
                        int kref= (dir==KDIR) ? kghost + koffset - 1 : k;

                        Vc(n,k,j,i) = Vc(n,kref,jref,iref);
                    });
                break;
            default:
                std::stringstream msg("Boundary condition type is not yet implemented");
                IDEFIX_ERROR(msg);

        }


    }

    Kokkos::Profiling::popRegion();

}

